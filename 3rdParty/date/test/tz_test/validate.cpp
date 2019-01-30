#include "date/tz.h"
#include <iostream>

void
test_info(const date::time_zone* zone, const date::sys_info& info)
{
    using namespace date;
    using namespace std::chrono;
    auto begin = info.begin;
    auto end = info.end - microseconds{1};
    auto mid = begin + (end - begin) /2;
    using sys_microseconds = sys_time<microseconds>;
    using zoned_microseconds = zoned_time<microseconds>;
    zoned_microseconds local{zone};

    if (begin > sys_days{jan/1/1700})
    {
        auto prev_local = local;
        local = begin;
        prev_local = begin - seconds{1};
        auto slocal = local.get_local_time();
        auto plocal = prev_local.get_local_time();
        if (plocal < slocal - seconds{1})
        {
            assert(sys_microseconds{local} == begin);
            try
            {
                local = plocal + (slocal - seconds{1} - plocal) / 2;
                assert(false);
            }
            catch (const nonexistent_local_time&)
            {
            }
        }
        else if (plocal > slocal - seconds{1})
        {
            try
            {
                local = slocal - seconds{1} + (plocal - (slocal - seconds{1})) / 2;
                assert(false);
            }
            catch (const ambiguous_local_time&)
            {
            }
        }
    }

    local = mid;
    assert(sys_microseconds{local} == mid);

    if (end < sys_days{jan/1/3000})
    {
        local = end;
        auto next_local = local;
        next_local = info.end;
        auto slocal = local.get_local_time();
        auto nlocal = next_local.get_local_time();
        if (nlocal < slocal + microseconds{1})
        {
            try
            {
                local = nlocal + (slocal + microseconds{1} - nlocal) / 2;
                assert(false);
            }
            catch (const ambiguous_local_time&)
            {
            }
        }
        else if (nlocal > slocal + microseconds{1})
        {
            assert(sys_microseconds{local} == end);
            try
            {
                local = slocal + microseconds{1} +
                        (nlocal - (slocal + microseconds{1})) / 2;
                assert(false);
            }
            catch (const nonexistent_local_time&)
            {
            }
        }
    }
}

void
tzmain()
{
    using namespace date;
    using namespace std::chrono;
    auto& db = get_tzdb();
    std::vector<std::string> names;
#if USE_OS_TZDB
    names.reserve(db.zones.size());
    for (auto& zone : db.zones)
        names.push_back(zone.name());
#else  // !USE_OS_TZDB
    names.reserve(db.zones.size() + db.links.size());
    for (auto& zone : db.zones)
        names.push_back(zone.name());
    for (auto& link : db.links)
        names.push_back(link.name());
    std::sort(names.begin(), names.end());
#endif  // !USE_OS_TZDB
    std::cout << db.version << "\n\n";
    for (auto const& name : names)
    {
        std::cout << name << '\n';
        auto z = locate_zone(name);
        auto begin = sys_days(jan/1/year::min()) + seconds{0};
        auto end   = sys_days(jan/1/2035) + seconds{0};
        auto info = z->get_info(begin);
        std::cout << "Initially:           ";
        if (info.offset >= seconds{0})
            std::cout << '+';
        std::cout << make_time(info.offset);
        if (info.save == minutes{0})
            std::cout << " standard ";
        else
            std::cout << " daylight ";
        std::cout << info.abbrev << '\n';
        test_info(z, info);
        auto prev_offset = info.offset;
        auto prev_abbrev = info.abbrev;
        auto prev_save = info.save;
        for (begin = info.end; begin < end; begin = info.end)
        {
            info = z->get_info(begin);
            test_info(z, info);
            if (info.offset == prev_offset && info.abbrev == prev_abbrev &&
                    info.save == prev_save)
                continue;
            auto dp = floor<days>(begin);
            auto ymd = year_month_day(dp);
            auto time = make_time(begin - dp);
            std::cout << ymd << ' ' << time << "Z ";
            if (info.offset >= seconds{0})
                std::cout << '+';
            std::cout << make_time(info.offset);
            if (info.save == minutes{0})
                std::cout << " standard ";
            else
                std::cout << " daylight ";
            std::cout << info.abbrev << '\n';
            prev_offset = info.offset;
            prev_abbrev = info.abbrev;
            prev_save = info.save;
        }
        std::cout << '\n';
    }
}

int
main()
{
    try
    {
        tzmain();
    }
    catch(const std::exception& ex)
    {
        std::cout << "An exception occured: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
