// (c) Copyright Juergen Hunold 2016
// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_TEST_MODULE QtPurchasing

#include <QtPurchasing>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE (defines)
{
    BOOST_CHECK_EQUAL(BOOST_IS_DEFINED(QT_CORE_LIB), true);
    BOOST_CHECK_EQUAL(BOOST_IS_DEFINED(QT_PURCHASING_LIB), true);
}

class DummyProduct : public QInAppProduct
{
public:

    DummyProduct() : QInAppProduct{QStringLiteral("One"),
                                   QString{},
                                   QString{},
                                   Consumable,
                                   QStringLiteral("DummyProduct"),
                                   nullptr} {};
    void purchase() override {};
};

std::ostream&
operator << (std::ostream& stream, QString const& string)
{
    stream << qPrintable(string);
    return stream;
}

BOOST_AUTO_TEST_CASE (purchase)
{
  DummyProduct product;

  BOOST_TEST(product.price() == QLatin1String("One"));
  BOOST_TEST(product.identifier() == QLatin1String("DummyProduct"));
}
