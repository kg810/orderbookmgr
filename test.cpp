#undef BOOST_TEST_NO_MAIN
#define BOOST_TEST_MAIN

#include<boost/test/included/unit_test.hpp>

#include"orderbook.h"

BOOST_AUTO_TEST_CASE(  order_add_remove )
{
    ob::OrderBookMgr bmgr;
    ob::Order order{111, 'B', 86.5, 10000, std::string("test.hk")};
    bmgr.add(order);
/* current book status
----------------------------------------------------
|         Bid            |            Offer        |
----------------------------------------------------
|count  quantity  price  |   price  quantity  count|
----------------------------------------------------
|1      10000     86.5   |                         |
----------------------------------------------------
*/

    BOOST_CHECK_EQUAL(bmgr.orderSize(), 1);
    BOOST_CHECK_EQUAL(bmgr.bookSize(), 1);
    BOOST_CHECK(bmgr.orderExist(111));

    std::hash<std::string> hash_to_bookid;
    int bookId = hash_to_bookid("test.hk");

    BOOST_CHECK(bmgr.bookExist(bookId));
    auto bookptr = bmgr.getBook(bookId);
    BOOST_CHECK_EQUAL(bookptr->bidsSize(), 1);
    BOOST_CHECK_EQUAL(bookptr->bids[0].price, 86.5);
    BOOST_CHECK_EQUAL(bookptr->bids[0].quantity, 10000);
    BOOST_CHECK_EQUAL(bookptr->bids[0].count, 1);

    // full fill bid side
    order.id = 112; order.price = 86.4; bmgr.add(order); BOOST_CHECK_EQUAL(bookptr->bidsSize(), 2);
    order.id = 113; order.price = 86.3; bmgr.add(order); BOOST_CHECK_EQUAL(bookptr->bidsSize(), 3);
    order.id = 114; order.price = 86.2; bmgr.add(order); BOOST_CHECK_EQUAL(bookptr->bidsSize(), 4);
    order.id = 115; order.price = 86.1; bmgr.add(order); BOOST_CHECK_EQUAL(bookptr->bidsSize(), 5);

    // full fill offer side
    order.side = 'O';
    order.id = 117; order.price = 86.6; bmgr.add(order); BOOST_CHECK_EQUAL(bookptr->offersSize(), 1);
    order.id = 118; order.price = 86.7; bmgr.add(order); BOOST_CHECK_EQUAL(bookptr->offersSize(), 2);
    order.id = 119; order.price = 86.8; bmgr.add(order); BOOST_CHECK_EQUAL(bookptr->offersSize(), 3);
    order.id = 120; order.price = 86.9; bmgr.add(order); BOOST_CHECK_EQUAL(bookptr->offersSize(), 4);
    order.id = 121; order.price = 87.0; bmgr.add(order); BOOST_CHECK_EQUAL(bookptr->offersSize(), 5);

/* current book status
----------------------------------------------------
|         Bid            |            Offer        |
----------------------------------------------------
|count  quantity  price  |   price  quantity  count|
----------------------------------------------------
|1      10000     86.5   |   86.6   10000     1    |
|1      10000     86.4   |   86.7   10000     1    |
|1      10000     86.3   |   86.8   10000     1    |
|1      10000     86.2   |   86.9   10000     1    |
|1      10000     86.1   |   87.0   10000     1    |
----------------------------------------------------
*/

    // add quantity to existing level
    order.id = 122; order.price = 86.7; bmgr.add(order);
/* current book status
----------------------------------------------------
|         Bid            |            Offer        |
----------------------------------------------------
|count  quantity  price  |   price  quantity  count|
----------------------------------------------------
|1      10000     86.5   |   86.6   10000     1    |
|1      10000     86.4   |   86.7   20000     2    |
|1      10000     86.3   |   86.8   10000     1    |
|1      10000     86.2   |   86.9   10000     1    |
|1      10000     86.1   |   87.0   10000     1    |
----------------------------------------------------
*/
    BOOST_CHECK_EQUAL(bookptr->offers[1].quantity, 20000);
    BOOST_CHECK_EQUAL(bookptr->offers[1].count, 2);

    // insert new level and remove lowest level if exceed MaxLevel
    order.id = 123; order.price = 86.72; bmgr.add(order);
/* current book status
----------------------------------------------------
|         Bid            |            Offer        |
----------------------------------------------------
|count  quantity  price  |   price  quantity  count|
----------------------------------------------------
|1      10000     86.5   |   86.6   10000     1    |
|1      10000     86.4   |   86.7   20000     2    |
|1      10000     86.3   |   86.72  10000     1    |
|1      10000     86.2   |   86.8   10000     1    |
|1      10000     86.1   |   86.9   10000     1    |
|                        |   87.0   10000     1    |
----------------------------------------------------
*/
    BOOST_CHECK_EQUAL(bookptr->offersSize(), 6);
    BOOST_CHECK(bookptr->levelQty<false>(86.72)); // new level inserted
    //BOOST_CHECK(!bookptr->levelQty<false>(87.0)); // old lowest level removed

    // remove order 122 to reduce quantity on level of price 86.7
    bmgr.remove(122);
/* current book status
----------------------------------------------------
|         Bid            |            Offer        |
----------------------------------------------------
|count  quantity  price  |   price  quantity  count|
----------------------------------------------------
|1      10000     86.5   |   86.6   10000     1    |
|1      10000     86.4   |   86.7   10000     1    |
|1      10000     86.3   |   86.72  10000     1    |
|1      10000     86.2   |   86.8   10000     1    |
|1      10000     86.1   |   86.9   10000     1    |
|                        |   87.0   10000     1    |
----------------------------------------------------
*/
    BOOST_CHECK_EQUAL(bookptr->offers[1].quantity, 10000);
    BOOST_CHECK_EQUAL(bookptr->offers[1].count, 1);

    // remove order 118 to remove level on price 86.7
    bmgr.remove(118);
/* current book status
----------------------------------------------------
|         Bid            |            Offer        |
----------------------------------------------------
|count  quantity  price  |   price  quantity  count|
----------------------------------------------------
|1      10000     86.5   |   86.6   10000     1    |
|1      10000     86.4   |   86.72  10000     1    |
|1      10000     86.3   |   86.8   10000     1    |
|1      10000     86.2   |   86.9   10000     1    |
|1      10000     86.1   |   87.0   10000     1    |
----------------------------------------------------
*/
    BOOST_CHECK(!bookptr->levelQty<false>(86.7));
    BOOST_CHECK_EQUAL(bookptr->offersSize(), 5); // removed one level, only 5 left

    // replace order 117 with new quantity 8000
    bmgr.replace(117, 8000);
/* current book status
----------------------------------------------------
|         Bid            |            Offer        |
----------------------------------------------------
|count  quantity  price  |   price  quantity  count|
----------------------------------------------------
|1      10000     86.5   |   86.6   8000      1    |
|1      10000     86.4   |   86.72  10000     1    |
|1      10000     86.3   |   86.8   10000     1    |
|1      10000     86.2   |   86.9   10000     1    |
|1      10000     86.1   |   87.0   10000     1    |
----------------------------------------------------
*/
    BOOST_CHECK_EQUAL(bookptr->levelQty<false>(86.6), 8000);

    // query price for symbol test.hk, bid side, level 2
    BOOST_CHECK_EQUAL(bmgr.priceOfSideLevel('B', 2, "test.hk"), 86.3);
    // query price for symbol test.hk, bid side, level 6 - exceed max level!
    BOOST_CHECK_EQUAL(bmgr.priceOfSideLevel('B', 6, "test.hk"), 0);


    // query quantity for symbol test.hk, bid side, level 4
    BOOST_CHECK_EQUAL(bmgr.qtyOfSideLevel('B', 4, "test.hk"), 10000);
    // query quantity for symbol test.hk, bid side, level 6 - exceed max level!
    BOOST_CHECK_EQUAL(bmgr.qtyOfSideLevel('B', 6, "test.hk"), 0);
}
