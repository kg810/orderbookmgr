/*
Basic idea is construct a book for each symbol.
Each time we receive an order, will consolidate corresponding book with the order price/qty.
A sample book looks like below:
----------------------------------------------------
|         Bid            |            Offer        |
----------------------------------------------------
|count  quantity  price  |   price  quantity  count|
----------------------------------------------------
|10     100000    16.1   |   16.2   60000     3    |
|20     110000    16.0   |   16.4   75000     8    |
|                       ...                        |
----------------------------------------------------
*/

#ifndef _ORDERBOOK_
#define _ORDERBOOK_

#include<cstddef>
#include<vector>
#include<unordered_map>
#include<iostream>
#include<string>
#include<memory>
#include<algorithm>

namespace ob {

struct Book;

struct Order
{
    int id;
    char side;
    double price;
    int quantity;

    std::string symbol;

    Order() = default;
    Order(int _id, char _side, double _price, int _qty, std::string _sym)
        : id(_id), side(_side), price(_price), quantity(_qty), symbol(_sym)
    {}

    int getId() const { return id; }
};

struct Book
{
    struct Level
    {
        double price;
        int quantity;
        int count; // count of orders on each level

        Level() = default;
        Level(double _price, int _qty, int _cnt)
            : price(_price), quantity(_qty), count(_cnt)
        {}
    };

    int id;
    std::vector<Level> bids;
    std::vector<Level> offers;

    Book() = delete;
    Book(const Book&) = delete;
    Book& operator=(const Book&) = delete;

    Book(int _id) : id(_id)
    {
        size_t sizeOfSide = sizeof(Book) * 10; // reserve 10 level space
        bids.reserve(sizeOfSide);
        offers.reserve(sizeOfSide);
    }

    bool add(char side, double price, int quantity)
    {
        if(side == 'B')
        {
            return add<true>(bids, price, quantity);
        }
        else
        {
            return add<false>(offers, price, quantity);
        }
    }

    bool remove(char side, double price, int quantity)
    {
        if(side == 'B')
        {
            return remove<true>(bids, price, quantity);
        }
        else
        {
            return remove<false>(offers, price, quantity);
        }
    }

    bool replace(char side, double price, int quantity)
    {
        if(side == 'B')
        {
            return replace<true>(bids, price, quantity);
        }
        else
        {
            return replace<false>(offers, price, quantity);
        }
    }

    template<bool isBid>
    static bool lt(const Level& l, const double p) { return isBid ? (p < l.price) : (p > l.price); }

    template<bool isBid, typename CONT>
    bool add(CONT& cont, double price, int quantity)
    {
        if(price > 0 && quantity > 0)
        {
            auto itr = std::lower_bound(cont.begin(), cont.end(), price, lt<isBid>);
            if(itr != cont.end())
            {
                if(price == itr->price) // modify
                {
                    itr->quantity += quantity;
                    ++(itr->count);
                }
                else // insert and shift
                {
                    cont.insert(itr, {price, quantity, 1});
                }
            }
            else
            {
                cont.push_back({price, quantity, 1});
            }
            return true;
        }
        else
        {
            std::cerr << "unqualified price(" << price << ") or quantity(" << quantity << ")" << std::endl;
            return false;
        }
    }

    template<bool isBid, typename CONT>
    bool remove(CONT& cont, double price, int quantity)
    {
        if(levelQty<isBid>(price)) // level exist
        {
            auto itr = std::lower_bound(cont.begin(), cont.end(), price, lt<isBid>);

            if(itr->count == 1) // remove the level
            {
                cont.erase(itr);
            }
            else // reduce level quantity of order quantity
            {
                itr->quantity -= quantity;
                --(itr->count);
            }
            return true;
        }
        else
        {
            std::cerr << "no level on price " << price << std::endl;
            return false;
        }
    }

    template<bool isBid, typename CONT>
    bool replace(CONT& cont, double price, int quantity)
    {
        if(levelQty<isBid>(price)) // level exist
        {
            auto itr = std::lower_bound(cont.begin(), cont.end(), price, lt<isBid>);

            itr->quantity = quantity;

            return true;
        }
        else
        {
            std::cerr << "no level on price " << price << std::endl;
            return false;
        }
    }

    int bidsSize() const { return bids.size(); }
    int offersSize() const { return offers.size(); }
    int getId() const { return id; }

    template<bool isBid>
    const int levelQty(const double price) const
    {
        auto& cont = isBid ? bids : offers;
        auto itr = std::lower_bound(cont.begin(), cont.end(), price, lt<isBid>);
        if(itr != cont.end() && itr->price == price) return itr->quantity;
        return 0;
    }

};

class OrderBookMgr
{
    std::unordered_map<int, Order> orders;
    std::unordered_map<int, std::shared_ptr<Book>> books;
    std::hash<std::string> hash_to_bookid;

public:
    OrderBookMgr() = default;

    std::shared_ptr<Book> getBook(int bookId)
    {
        auto itr = books.find(bookId);
        if(itr != books.end()) return itr->second;
        return nullptr;
    }

    // add a new order
    bool add(Order& order)
    {
        if(!orderExist(order.id))
        {
            int bookId = hash_to_bookid(order.symbol);

            if(!bookExist(bookId)) // create and init book
            {
                std::shared_ptr<Book> bptr = std::make_shared<Book>(bookId);
                if(bptr->add(order.side, order.price, order.quantity))
                {
                    books.emplace(bookId, bptr);
                    orders.emplace(order.id, order);
                    return true;
                }

                return false;
            }
            else // update existing book
            {
                auto itr = books.find(bookId);
                if(itr->second->add(order.side, order.price, order.quantity))
                {
                    orders.emplace(order.id, order);
                    return true;
                }
                return false;
            }
        }
        else
        {
            std::cerr << "order " << order.id << " already exists" << std::endl;
            return false;
        }
    }

    // remove given order
    bool remove(int orderId)
    {
        if(orderExist(orderId))
        {
            auto itr = orders.find(orderId);
            auto order = itr->second;

            int bookId = hash_to_bookid(order.symbol);
            auto bptr = getBook(bookId);
            if(bptr->remove(order.side, order.price, order.quantity))
            {
                orders.erase(itr);
                return true;
            }

            return false;

        }
        else
        {
            std::cerr << "no order on orderId " << orderId << std::endl;
            return false;
        }
    }

    // replace given order
    bool replace(int orderId, int quantity)
    {
        if(orderExist(orderId))
        {
            auto itr = orders.find(orderId);
            auto order = itr->second;

            int bookId = hash_to_bookid(order.symbol);
            auto bptr = getBook(bookId);

            int levelQty;
            if(order.side == 'B')
                levelQty = bptr->levelQty<true>(order.price);
            else
                levelQty = bptr->levelQty<false>(order.price);

            int newQty = levelQty - order.quantity + quantity;

            if(bptr->replace(order.side, order.price, newQty))
            {
                order.quantity = quantity;
                return true;
            }

            return false;

        }
        else
        {
            std::cerr << "no order on orderId " << orderId << std::endl;
            return false;
        }
    }

    double priceOfSideLevel(char side, int levelIndex, std::string symbol)
    {
        int bookId = hash_to_bookid(symbol);
        if(bookExist(bookId))
        {
            auto bptr = books.find(bookId)->second;
            if(side == 'B')
            {
                if(levelIndex < bptr->bidsSize())
                {
                    return bptr->bids[levelIndex].price;
                }
                else
                {
                    std::cerr << "levelIndex out of range." << std::endl;
                    return 0;
                }

            }
            else
            {
                if(levelIndex < bptr->offersSize())
                {
                    return bptr->offers[levelIndex].price;
                }
                else
                {
                    std::cerr << "levelIndex out of range." << std::endl;
                    return 0;
                }

            }

        }
        else
        {
            std::cerr << "no book on bookId " << bookId << std::endl;
            return 0;
        }
    }

    int qtyOfSideLevel(char side, int levelIndex, std::string symbol)
    {
        int bookId = hash_to_bookid(symbol);
        if(bookExist(bookId))
        {
            auto bptr = books.find(bookId)->second;
            if(side == 'B')
            {
                if(levelIndex < bptr->bidsSize())
                {
                    return bptr->bids[levelIndex].quantity;
                }
                else
                {
                    std::cerr << "levelIndex out of range." << std::endl;
                    return 0;
                }

            }
            else
            {
                if(levelIndex < bptr->offersSize())
                {
                    return bptr->offers[levelIndex].quantity;
                }
                else
                {
                    std::cerr << "levelIndex out of range." << std::endl;
                    return 0;
                }

            }

        }
        else
        {
            std::cerr << "no book on bookId " << bookId << std::endl;
            return 0;
        }

    }

    bool orderExist(int orderId)
    {
        if(orders.find(orderId) != orders.end())
            return true;

        return false;
    }

    bool bookExist(int bookId)
    {
        if(books.find(bookId) != books.end())
            return true;
        return false;
    }

    int orderSize() { return orders.size(); }
    int bookSize() { return books.size(); }

};

}

#endif
