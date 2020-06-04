// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#ifndef V8_OBJECTS_JS_DATE_TIME_FORMAT_H_
#define V8_OBJECTS_JS_DATE_TIME_FORMAT_H_

#include <set>
#include <string>

#include "src/execution/isolate.h"
#include "src/objects/intl-objects.h"
#include "src/objects/managed.h"
#include "torque-generated/field-offsets-tq.h"
#include "unicode/uversion.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace U_ICU_NAMESPACE {
class DateIntervalFormat;
class Locale;
class SimpleDateFormat;
}  // namespace U_ICU_NAMESPACE

namespace v8 {
namespace internal {

class JSDateTimeFormat : public JSObject {
 public:
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSDateTimeFormat> New(
      Isolate* isolate, Handle<Map> map, Handle<Object> locales,
      Handle<Object> options, const char* service);

  V8_WARN_UNUSED_RESULT static MaybeHandle<JSObject> ResolvedOptions(
      Isolate* isolate, Handle<JSDateTimeFormat> date_time_format);

  // ecma402/#sec-unwrapdatetimeformat
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSDateTimeFormat>
  UnwrapDateTimeFormat(Isolate* isolate, Handle<JSReceiver> format_holder);

  // Convert the options to ICU DateTimePatternGenerator skeleton.
  static Maybe<std::string> OptionsToSkeleton(Isolate* isolate,
                                              Handle<JSReceiver> options);

  // Return the time zone id which match ICU's expectation of title casing
  // return empty string when error.
  static std::string CanonicalizeTimeZoneID(Isolate* isolate,
                                            const std::string& input);

  // ecma402/#sec-datetime-format-functions
  // DateTime Format Functions
  V8_WARN_UNUSED_RESULT static MaybeHandle<String> DateTimeFormat(
      Isolate* isolate, Handle<JSDateTimeFormat> date_time_format,
      Handle<Object> date);

  // ecma402/#sec-Intl.DateTimeFormat.prototype.formatToParts
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSArray> FormatToParts(
      Isolate* isolate, Handle<JSDateTimeFormat> date_time_format,
      double date_value);

  // ecma402/#sec-intl.datetimeformat.prototype.formatRange
  V8_WARN_UNUSED_RESULT static MaybeHandle<String> FormatRange(
      Isolate* isolate, Handle<JSDateTimeFormat> date_time_format,
      double x_date_value, double y_date_value);

  // ecma402/sec-Intl.DateTimeFormat.prototype.formatRangeToParts
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSArray> FormatRangeToParts(
      Isolate* isolate, Handle<JSDateTimeFormat> date_time_format,
      double x_date_value, double y_date_value);

  // ecma-402/#sec-todatetimeoptions
  enum class RequiredOption { kDate, kTime, kAny };
  enum class DefaultsOption { kDate, kTime, kAll };
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSObject> ToDateTimeOptions(
      Isolate* isolate, Handle<Object> input_options, RequiredOption required,
      DefaultsOption defaults);

  V8_WARN_UNUSED_RESULT static MaybeHandle<String> ToLocaleDateTime(
      Isolate* isolate, Handle<Object> date, Handle<Object> locales,
      Handle<Object> options, RequiredOption required, DefaultsOption defaults,
      const char* method);

  V8_EXPORT_PRIVATE static const std::set<std::string>& GetAvailableLocales();

  Handle<String> HourCycleAsString() const;
  DECL_CAST(JSDateTimeFormat)

  // ecma-402/#sec-properties-of-intl-datetimeformat-instances
  enum class DateTimeStyle { kUndefined, kFull, kLong, kMedium, kShort };

// Layout description.
  DEFINE_FIELD_OFFSET_CONSTANTS(JSObject::kHeaderSize,
                                TORQUE_GENERATED_JS_DATE_TIME_FORMAT_FIELDS)

  inline void set_hour_cycle(Intl::HourCycle hour_cycle);
  inline Intl::HourCycle hour_cycle() const;

  inline void set_date_style(DateTimeStyle date_style);
  inline DateTimeStyle date_style() const;

  inline void set_time_style(DateTimeStyle time_style);
  inline DateTimeStyle time_style() const;

// Bit positions in |flags|.
#define FLAGS_BIT_FIELDS(V, _)            \
  V(HourCycleBits, Intl::HourCycle, 3, _) \
  V(DateStyleBits, DateTimeStyle, 3, _)   \
  V(TimeStyleBits, DateTimeStyle, 3, _)

  DEFINE_BIT_FIELDS(FLAGS_BIT_FIELDS)
#undef FLAGS_BIT_FIELDS

  STATIC_ASSERT(Intl::HourCycle::kUndefined <= HourCycleBits::kMax);
  STATIC_ASSERT(Intl::HourCycle::kH11 <= HourCycleBits::kMax);
  STATIC_ASSERT(Intl::HourCycle::kH12 <= HourCycleBits::kMax);
  STATIC_ASSERT(Intl::HourCycle::kH23 <= HourCycleBits::kMax);
  STATIC_ASSERT(Intl::HourCycle::kH24 <= HourCycleBits::kMax);

  STATIC_ASSERT(DateTimeStyle::kUndefined <= DateStyleBits::kMax);
  STATIC_ASSERT(DateTimeStyle::kFull <= DateStyleBits::kMax);
  STATIC_ASSERT(DateTimeStyle::kLong <= DateStyleBits::kMax);
  STATIC_ASSERT(DateTimeStyle::kMedium <= DateStyleBits::kMax);
  STATIC_ASSERT(DateTimeStyle::kShort <= DateStyleBits::kMax);

  STATIC_ASSERT(DateTimeStyle::kUndefined <= TimeStyleBits::kMax);
  STATIC_ASSERT(DateTimeStyle::kFull <= TimeStyleBits::kMax);
  STATIC_ASSERT(DateTimeStyle::kLong <= TimeStyleBits::kMax);
  STATIC_ASSERT(DateTimeStyle::kMedium <= TimeStyleBits::kMax);
  STATIC_ASSERT(DateTimeStyle::kShort <= TimeStyleBits::kMax);

  DECL_ACCESSORS(icu_locale, Managed<icu::Locale>)
  DECL_ACCESSORS(icu_simple_date_format, Managed<icu::SimpleDateFormat>)
  DECL_ACCESSORS(icu_date_interval_format, Managed<icu::DateIntervalFormat>)
  DECL_ACCESSORS(bound_format, Object)
  DECL_INT_ACCESSORS(flags)

  DECL_PRINTER(JSDateTimeFormat)
  DECL_VERIFIER(JSDateTimeFormat)

  OBJECT_CONSTRUCTORS(JSDateTimeFormat, JSObject);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_DATE_TIME_FORMAT_H_
