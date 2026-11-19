#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <cstdint>
#include <cassert>

/*
 * Page Allocator Data Preprocessor (C++ Version)
 *
 * This tool processes real allocation traces (collected via BPF)
 * and converts them into a format suitable for the PA simulator.
 *
 * Supported input formats:
 *   HPA: shard_ind_int,addr_int,nsecs_int,probe,size_int
 *   SEC: process_id,thread_id,thread_name,nsecs_int,_c4,sec_ptr_int,sec_shard_ptr_int,edata_ptr_int,size_int,is_frequent_reuse_int
 *
 * Output format (5 columns):
 *   shard_ind_int,operation_index,size_or_alloc_index,nsecs,is_frequent
 *   where:
 *   - shard_ind_int: shard index as integer
 *   - operation_index: 0=alloc, 1=dalloc
 *   - size_or_alloc_index: for alloc operations show bytes,
 *                          for dalloc operations show index of corresponding alloc
 *   - nsecs: nanonosec of some monotonic clock
 *   - is_frequent: 1 if frequent reuse allocation, 0 otherwise
 */

enum class TraceFormat { HPA, SEC };

struct TraceEvent {
	int         shard_ind;
	uintptr_t   addr;
	uint64_t    nsecs;
	std::string probe;
	size_t      size;
	bool        is_frequent;
};

struct AllocationRecord {
	uintptr_t addr;
	size_t    size;
	int       shard_ind;
	size_t    alloc_index;
	uint64_t  nsecs;
};

class AllocationTracker {
      private:
	std::unordered_map<uintptr_t, AllocationRecord> records_;

      public:
	void
	add_allocation(uintptr_t addr, size_t size, int shard_ind,
	    size_t alloc_index, uint64_t nsecs) {
		records_[addr] = {addr, size, shard_ind, alloc_index, nsecs};
	}

	AllocationRecord *
	find_allocation(uintptr_t addr) {
		auto it = records_.find(addr);
		return (it != records_.end()) ? &it->second : nullptr;
	}

	void
	remove_allocation(uintptr_t addr) {
		records_.erase(addr);
	}

	size_t
	count() const {
		return records_.size();
	}
};

class ArenaMapper {
      private:
	std::unordered_map<uintptr_t, int> sec_ptr_to_arena_;
	int                                next_arena_index_;

      public:
	ArenaMapper() : next_arena_index_(0) {}

	int
	get_arena_index(uintptr_t sec_ptr) {
		if (sec_ptr == 0) {
			/* Should not be seeing null sec pointer anywhere. Use this as a sanity check.*/
			return 0;
		}

		auto it = sec_ptr_to_arena_.find(sec_ptr);
		if (it != sec_ptr_to_arena_.end()) {
			return it->second;
		}

		/* New sec_ptr, assign next available arena index */
		int arena_index = next_arena_index_++;
		sec_ptr_to_arena_[sec_ptr] = arena_index;
		return arena_index;
	}

	size_t
	arena_count() const {
		return sec_ptr_to_arena_.size();
	}
};

bool
is_alloc_operation(const std::string &probe) {
	return (probe == "hpa_alloc" || probe == "sec_alloc");
}

bool
is_dalloc_operation(const std::string &probe) {
	return (probe == "hpa_dalloc" || probe == "sec_dalloc");
}

bool
parse_hpa_line(const std::string &line, TraceEvent &event) {
	std::istringstream ss(line);
	std::string        token;

	/* Parse shard_ind_int */
	if (!std::getline(ss, token, ',')) {
		return true;
	}
	event.shard_ind = std::stoi(token);

	/* Parse addr_int */
	if (!std::getline(ss, token, ',')) {
		return true;
	}
	event.addr = std::stoull(token);

	/* Parse nsecs_int */
	if (!std::getline(ss, token, ',')) {
		return true;
	}
	event.nsecs = std::stoull(token);

	/* Parse probe */
	if (!std::getline(ss, token, ',')) {
		return true;
	}
	event.probe = token;

	/* Parse size_int */
	if (!std::getline(ss, token, ',')) {
		return true;
	}
	event.size = std::stoull(token);

	/* HPA format doesn't have is_frequent field, set default */
	event.is_frequent = true;

	return false;
}

bool
parse_sec_line(
    const std::string &line, TraceEvent &event, ArenaMapper &arena_mapper) {
	std::istringstream ss(line);
	std::string        token;

	/* Skip process_id */
	if (!std::getline(ss, token, ',')) {
		return true;
	}

	/* Skip thread_id */
	if (!std::getline(ss, token, ',')) {
		return true;
	}

	/* Skip thread_name */
	if (!std::getline(ss, token, ',')) {
		return true;
	}

	/* Parse nsecs_int */
	if (!std::getline(ss, token, ',')) {
		return true;
	}
	event.nsecs = std::stoull(token);

	/* Parse operation */
	if (!std::getline(ss, token, ',')) {
		return true;
	}

	event.probe = token;

	/* Parse sec_ptr_int (used for arena mapping) */
	uintptr_t sec_ptr = 0;
	if (!std::getline(ss, token, ',')) {
		return true;
	}
	if (!token.empty()) {
		sec_ptr = std::stoull(token);
	}

	/* Map sec_ptr to arena index */
	event.shard_ind = arena_mapper.get_arena_index(sec_ptr);

	/* Skip sec_shard_ptr_int */
	if (!std::getline(ss, token, ',')) {
		return true;
	}

	/* Parse edata_ptr_int (used as the address) */
	if (!std::getline(ss, token, ',')) {
		return true;
	}
	if (!token.empty()) {
		event.addr = std::stoull(token);
	} else {
		event.addr = 0;
	}

	/* Parse size_int */
	if (!std::getline(ss, token, ',')
	    && !is_dalloc_operation(event.probe)) {
		/* SEC format may not always have size for dalloc */
		return true;
	}
	if (!token.empty()) {
		event.size = std::stoull(token);
	} else {
		/* When no size given, this is a dalloc, size won't be used. */
		event.size = 0;
	}

	/* Parse is_frequent_reuse_int */
	if (!std::getline(ss, token, ',')
	    && !is_dalloc_operation(event.probe)) {
		return true;
	}
	if (!token.empty()) {
		event.is_frequent = (std::stoi(token) != 0);
	} else {
		/*
		 * When no is_frequent_reuse_int given, this is a dalloc,
		 * is_frequent won't be used.
		 */
		event.is_frequent = false;
	}

	return false;
}

void
write_output_header(std::ofstream &output) {
	output << "shard_ind,operation,size_or_alloc_index,nsecs,is_frequent\n";
}

void
write_output_event(std::ofstream &output, int shard_ind, int operation,
    size_t value, uint64_t nsecs, bool is_frequent) {
	output << shard_ind << "," << operation << "," << value << "," << nsecs
	       << "," << (is_frequent ? 1 : 0) << "\n";
}

size_t
process_trace_file(const std::string &input_filename,
    const std::string &output_filename, TraceFormat format) {
	std::ifstream input(input_filename);
	if (!input.is_open()) {
		std::cerr << "Failed to open input file: " << input_filename
		          << std::endl;
		return 0;
	}

	std::ofstream output(output_filename);
	if (!output.is_open()) {
		std::cerr << "Failed to open output file: " << output_filename
		          << std::endl;
		return 0;
	}

	AllocationTracker tracker;
	ArenaMapper       arena_mapper; /* For SEC format arena mapping */

	std::string line;
	size_t      line_count = 0;
	size_t      output_count = 0;
	size_t      alloc_sequence = 0; /* Sequential index for allocations */
	size_t      unmatched_frees = 0;

	write_output_header(output);
	std::cout << "Reading from: " << input_filename << std::endl;

	/* Skip header line */
	if (!std::getline(input, line)) {
		std::cerr << "Error: Empty input file" << std::endl;
		return 0;
	}

	while (std::getline(input, line)) {
		line_count++;

		/* Skip empty lines */
		if (line.empty()) {
			continue;
		}

		TraceEvent event;
		bool       parse_error = false;

		if (format == TraceFormat::HPA) {
			parse_error = parse_hpa_line(line, event);
		} else if (format == TraceFormat::SEC) {
			parse_error = parse_sec_line(line, event, arena_mapper);
		}

		if (parse_error) {
			continue;
		}

		if (is_alloc_operation(event.probe)) {
			/* This is an allocation */
			write_output_event(output, event.shard_ind, 0,
			    event.size, event.nsecs, event.is_frequent);

			/* Track this allocation with the current sequence number */
			tracker.add_allocation(event.addr, event.size,
			    event.shard_ind, alloc_sequence, event.nsecs);
			alloc_sequence++;
		} else if (is_dalloc_operation(event.probe)) {
			/* This is a deallocation. Ignore dalloc without a corresponding alloc. */
			AllocationRecord *record = tracker.find_allocation(
			    event.addr);

			if (record) {
				/* Validate timing: deallocation should happen after allocation */
				assert(event.nsecs >= record->nsecs);
				/* Found matching allocation with valid timing */
				write_output_event(output, event.shard_ind, 1,
				    record->alloc_index, event.nsecs,
				    event.is_frequent);
				tracker.remove_allocation(event.addr);
				output_count++; /* Count this deallocation */
			} else {
				unmatched_frees++;
			}
		} else {
			std::cerr << "Unknown operation: " << event.probe
			          << std::endl;
		}
	}

	std::cout << "Processed " << line_count << " lines" << std::endl;
	std::cout << "Unmatched frees: " << unmatched_frees << std::endl;
	std::cout << "Extracted " << output_count << " alloc/dalloc pairs"
	          << std::endl;
	std::cout << "Results written to: " << output_filename << std::endl;

	return output_count;
}

TraceFormat
parse_format(const std::string &format_str) {
	if (format_str == "hpa") {
		return TraceFormat::HPA;
	} else if (format_str == "sec") {
		return TraceFormat::SEC;
	} else {
		throw std::invalid_argument(
		    "Unknown format: " + format_str + ". Use 'hpa' or 'sec'");
	}
}

int
main(int argc, char *argv[]) {
	if (argc < 4 || argc > 5) {
		std::cerr << "Usage: " << argv[0]
		          << " <format> <input_csv_file> <output_file>"
		          << std::endl;
		std::cerr << std::endl;
		std::cerr << "Arguments:" << std::endl;
		std::cerr << "  format          - Input format: 'hpa' or 'sec'"
		          << std::endl;
		std::cerr
		    << "                    hpa: shard_ind_int,addr_int,nsecs_int,probe,size_int"
		    << std::endl;
		std::cerr
		    << "                    sec: process_id,thread_id,thread_name,nsecs_int,_c4,sec_ptr_int,sec_shard_ptr_int,edata_ptr_int,size_int,is_frequent_reuse_int"
		    << std::endl;
		std::cerr << "  input_csv_file  - Input CSV trace file"
		          << std::endl;
		std::cerr
		    << "  output_file     - Output file for simulator with format:"
		    << std::endl;
		std::cerr
		    << "                    shard_ind,operation,size_or_alloc_index,nsecs,is_frequent"
		    << std::endl;
		std::cerr << std::endl;
		std::cerr << "Output format:" << std::endl;
		std::cerr << "  - operation: 0=alloc, 1=dalloc" << std::endl;
		std::cerr
		    << "  - size_or_alloc_index: bytes for alloc, alloc index for dalloc"
		    << std::endl;
		return 1;
	}

	try {
		TraceFormat format = parse_format(argv[1]);
		std::string input_file = argv[2];
		std::string output_file = argv[3];

		size_t events_generated = process_trace_file(
		    input_file, output_file, format);

		if (events_generated == 0) {
			std::cerr
			    << "No events generated. Check input file format and filtering criteria."
			    << std::endl;
			return 1;
		}
		return 0;
	} catch (const std::exception &e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
}
