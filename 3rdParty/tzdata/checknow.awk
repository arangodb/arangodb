# Check zonenow.tab for consistency with primary data.

# Contributed by Paul Eggert.  This file is in the public domain.

function record_zone(zone, data) {
  if (zone) {
    zone_data[zone] = data
    zones[data] = zones[data] " " zone
  }
}

BEGIN {
  while (getline <zdump_table) {
    if ($0 ~ /^TZ/) {
      record_zone(zone, data)
      zone = $0
      sub(/.*\.dir\//, "", zone)
      sub(/\/\//, "/", zone)
      sub(/"/, "", zone)
      data = ""
    } else if ($0 ~ /./)
      data = data $0 "\n"
  }
  record_zone(zone, data)
  FS = "\t"
}

/^[^#]/ {
  zone = $3
  data = zone_data[zone]
  if (!data) {
    printf "%s: no data\n", zone
    status = 1
  } else {
    zone2 = zonenow[data]
    if (zone2) {
      printf "zones %s and %s identical from now on\n", zone, zone2
      status = 1
    } else
      zonenow[data] = zone
  }
}

END {
 for (zone in zone_data) {
    data = zone_data[zone]
    if (!zonenow[data]) {
      printf "zonenow.tab should have one of:%s\n", zones[data]
      zonenow[data] = zone # This suppresses duplicate diagnostics.
      status = 1
    }
 }
 exit status
}
