////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "infra/ResourceMonitorScope.h"

using namespace arangodb::infra;

TEST(ResourceMonitorScopeTest, thread_local_is_null_by_default) {
  EXPECT_EQ(nullptr, detail::currentResourceMonitor());
}

TEST(ResourceMonitorScopeTest, scope_restores_null_on_destruction) {
  StaticResourceMonitor monitor;
  {
    ResourceMonitorScope scope{ResourceMonitorRef{monitor}};
    EXPECT_EQ(&monitor, detail::currentResourceMonitor());
  }
  EXPECT_EQ(nullptr, detail::currentResourceMonitor());
}

TEST(ResourceMonitorScopeTest, nested_scopes_restore_correctly) {
  StaticResourceMonitor outer;
  StaticResourceMonitor inner;

  {
    ResourceMonitorScope outerScope{ResourceMonitorRef{outer}};
    EXPECT_EQ(&outer, detail::currentResourceMonitor());

    {
      ResourceMonitorScope innerScope{ResourceMonitorRef{inner}};
      EXPECT_EQ(&inner, detail::currentResourceMonitor());
    }

    EXPECT_EQ(&outer, detail::currentResourceMonitor());
  }

  EXPECT_EQ(nullptr, detail::currentResourceMonitor());
}

TEST(ResourceMonitorScopeTest, scope_with_dynamic_monitor_manages_refcount) {
  ResourceMonitor monitor;
  EXPECT_EQ(0u, monitor.refCount());

  {
    ResourceMonitorScope scope{ResourceMonitorRef{monitor}};
    EXPECT_EQ(&monitor, detail::currentResourceMonitor());
    EXPECT_EQ(1u, monitor.refCount());
  }

  EXPECT_EQ(0u, monitor.refCount());
  EXPECT_EQ(nullptr, detail::currentResourceMonitor());
}

TEST(ResourceMonitorScopeTest,
     nested_scopes_with_dynamic_monitor_manage_refcount) {
  ResourceMonitor monitor;
  EXPECT_EQ(0u, monitor.refCount());

  {
    ResourceMonitorScope outerScope{ResourceMonitorRef{monitor}};
    EXPECT_EQ(1u, monitor.refCount());

    {
      ResourceMonitorScope innerScope{ResourceMonitorRef{monitor}};
      EXPECT_EQ(2u, monitor.refCount());
    }

    EXPECT_EQ(1u, monitor.refCount());
  }

  EXPECT_EQ(0u, monitor.refCount());
}

TEST(ResourceMonitorScopeTest,
     scope_with_dynamic_monitor_releases_ref_on_destruction) {
  ResourceMonitor monitor;

  {
    ResourceMonitorRef ref(monitor);
    EXPECT_EQ(1u, monitor.refCount());

    {
      ResourceMonitorScope scope(std::move(ref));
      EXPECT_EQ(1u, monitor.refCount());
      EXPECT_EQ(&monitor, detail::currentResourceMonitor());
    }

    EXPECT_EQ(0u, monitor.refCount());
  }
}

TEST(ResourceMonitorScopeTest, scope_with_null_ref_sets_thread_local_to_null) {
  StaticResourceMonitor monitor;
  {
    ResourceMonitorScope outerScope{ResourceMonitorRef{monitor}};
    EXPECT_EQ(&monitor, detail::currentResourceMonitor());

    {
      ResourceMonitorScope innerScope{ResourceMonitorRef{}};
      EXPECT_EQ(nullptr, detail::currentResourceMonitor());
    }

    EXPECT_EQ(&monitor, detail::currentResourceMonitor());
  }
  EXPECT_EQ(nullptr, detail::currentResourceMonitor());
}

TEST(ResourceMonitorScopeTest, nested_scopes_with_mixed_types_manage_refcount) {
  ResourceMonitor dynamic;
  StaticResourceMonitor staticMon;

  {
    ResourceMonitorScope outerScope{ResourceMonitorRef{dynamic}};
    EXPECT_EQ(1u, dynamic.refCount());
    EXPECT_EQ(&dynamic, detail::currentResourceMonitor());

    {
      ResourceMonitorScope innerScope{ResourceMonitorRef{staticMon}};
      EXPECT_EQ(&staticMon, detail::currentResourceMonitor());
      // the outer scope still owns the ref while its pointer is displaced
      EXPECT_EQ(1u, dynamic.refCount());
    }

    EXPECT_EQ(&dynamic, detail::currentResourceMonitor());
    EXPECT_EQ(1u, dynamic.refCount());
  }

  EXPECT_EQ(0u, dynamic.refCount());
  EXPECT_EQ(nullptr, detail::currentResourceMonitor());
}
