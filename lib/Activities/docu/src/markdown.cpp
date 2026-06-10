#include "markdown.h"

#include <algorithm>
#include <string>
#include <string_view>

namespace {

/**
 * Right-pad `text` with spaces to `width` (unchanged if already wider).
 */
auto pad(std::string_view text, std::size_t width) -> std::string {
  if (text.size() >= width) {
    return std::string(text);
  }
  return std::string(text) + std::string(width - text.size(), ' ');
}

/**
 * Render one record's fields as an org-style markdown table.
 *
 * Columns are padded to their widest cell so the pipes line up; the header
 * divider joins the columns with `+`.
 */
auto field_table(Struct const& record) -> std::string {
  auto const field_header = std::string_view{"Field"};
  auto const type_header = std::string_view{"Type"};

  auto field_width = field_header.size();
  auto type_width = type_header.size();
  for (auto const& field : record.fields) {
    field_width = std::max(field_width, field.name.size());
    type_width = std::max(type_width, field.type.size());
  }

  auto table = "| " + pad(field_header, field_width) + " | " +
               pad(type_header, type_width) + " |\n";
  table += "|" + std::string(field_width + 2, '-') + "+" +
           std::string(type_width + 2, '-') + "|\n";
  for (auto const& field : record.fields) {
    table += "| " + pad(field.name, field_width) + " | " +
             pad(field.type, type_width) + " |\n";
  }
  return table;
}

}  // namespace

auto activities_to_markdown(std::vector<ActivityDeclaration> const& activities)
    -> std::string {
  auto markdown = std::string{"# Activities\n"};
  for (auto const& activity : activities) {
    markdown += "\n## " + activity.type + "\n";
    markdown += "owner: " + activity.owner_file + ":" +
                std::to_string(activity.owner_line) + "\n";
    for (auto const& record : activity.data_type_definition) {
      markdown += "\n### " + record.name + "\n";
      if (not record.fields.empty()) {
        markdown += field_table(record);
      }
    }
  }
  return markdown;
}
