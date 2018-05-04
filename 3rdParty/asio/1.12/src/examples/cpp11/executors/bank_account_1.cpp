#include <asio/post.hpp>
#include <asio/thread_pool.hpp>
#include <iostream>

using asio::post;
using asio::thread_pool;

// Traditional active object pattern.
// Member functions do not block.

class bank_account
{
  int balance_ = 0;
  mutable thread_pool pool_{1};

public:
  void deposit(int amount)
  {
    post(pool_, [=]
      {
        balance_ += amount;
      });
  }

  void withdraw(int amount)
  {
    post(pool_, [=]
      {
        if (balance_ >= amount)
          balance_ -= amount;
      });
  }

  void print_balance() const
  {
    post(pool_, [=]
      {
        std::cout << "balance = " << balance_ << "\n";
      });
  }

  ~bank_account()
  {
    pool_.join();
  }
};

int main()
{
  bank_account acct;
  acct.deposit(20);
  acct.withdraw(10);
  acct.print_balance();
}
