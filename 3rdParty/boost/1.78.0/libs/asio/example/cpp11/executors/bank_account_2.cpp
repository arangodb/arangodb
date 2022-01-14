#include <boost/asio/execution.hpp>
#include <boost/asio/static_thread_pool.hpp>
#include <iostream>

using boost::asio::static_thread_pool;
namespace execution = boost::asio::execution;

// Traditional active object pattern.
// Member functions block until operation is finished.

class bank_account
{
  int balance_ = 0;
  mutable static_thread_pool pool_{1};

public:
  void deposit(int amount)
  {
    execution::execute(
        boost::asio::require(pool_.executor(),
          execution::blocking.always),
        [this, amount]
        {
          balance_ += amount;
        });
  }

  void withdraw(int amount)
  {
    execution::execute(
        boost::asio::require(pool_.executor(),
          execution::blocking.always),
        [this, amount]
        {
          if (balance_ >= amount)
            balance_ -= amount;
        });
  }

  int balance() const
  {
    int result = 0;
    execution::execute(
        boost::asio::require(pool_.executor(),
          execution::blocking.always),
        [this, &result]
        {
          result = balance_;
        });
    return result;
  }
};

int main()
{
  bank_account acct;
  acct.deposit(20);
  acct.withdraw(10);
  std::cout << "balance = " << acct.balance() << "\n";
}
