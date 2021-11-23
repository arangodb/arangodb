//  Copyright (c) 2018 Raffi Enficiaud
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE runtime_configuration2
#include <boost/test/included/unit_test.hpp>
using namespace boost::unit_test;

/// The interface with the device driver.
class DeviceInterface {
public:
    // acquires a specific device based on its name
    static DeviceInterface* factory(std::string const& device_name);
    virtual ~DeviceInterface(){}

    virtual bool setup() = 0;
    virtual bool teardown() = 0;
    virtual std::string get_device_name() const = 0;
};

class MockDevice: public DeviceInterface {
    bool setup() final {
        return true;
    }
    bool teardown() final {
        return true;
    }
    std::string get_device_name() const {
        return "mock_device";
    }
};

DeviceInterface* DeviceInterface::factory(std::string const& device_name) {
    if(device_name == "mock_device") {
        return new MockDevice();
    }
    return nullptr;
}

struct CommandLineDeviceInit {
  CommandLineDeviceInit() {
      BOOST_TEST_REQUIRE( framework::master_test_suite().argc == 3 );
      BOOST_TEST_REQUIRE( framework::master_test_suite().argv[1] == "--device-name" );
  }
  void setup() {
      device = DeviceInterface::factory(framework::master_test_suite().argv[2]);
      BOOST_TEST_REQUIRE(
        device != nullptr,
        "Cannot create the device " << framework::master_test_suite().argv[2] );
      BOOST_TEST_REQUIRE(
        device->setup(),
        "Cannot initialize the device " << framework::master_test_suite().argv[2] );
  }
  void teardown() {
      if(device) {
        BOOST_TEST(
          device->teardown(),
          "Cannot tear-down the device " << framework::master_test_suite().argv[2]);
      }
      delete device;
  }
  static DeviceInterface *device;
};
DeviceInterface* CommandLineDeviceInit::device = nullptr;

BOOST_TEST_GLOBAL_FIXTURE( CommandLineDeviceInit );

BOOST_AUTO_TEST_CASE(check_device_has_meaningful_name)
{
    BOOST_TEST(CommandLineDeviceInit::device->get_device_name() != "");
}
//]
