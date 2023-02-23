#include <scaffold/string_stream.h>
#include <scaffold/timed_block.h>

using namespace fo;
using namespace string_stream;

namespace timedblock {

RecordTable &get_table() {
    static RecordTable table;
    return table;
}

void print_record_table(FILE *f) {
    Buffer ss(memory_globals::default_allocator());
    ss << "Block";
    tab(ss, 40);
    ss << "TimeSpent(ms)";
    tab(ss, 60);
    ss << "TimesEntered";
    tab(ss, 90);
    ss << "MaxDurations(ms)";
    tab(ss, 105);
    ss << "\n";

    repeat(ss, 10, '-');
    tab(ss, 40);

    repeat(ss, 10, '-');
    tab(ss, 60);

    repeat(ss, 10, '-');
    tab(ss, 90);

    repeat(ss, 10, '-');
    tab(ss, 95);
    ss << "\n";

    auto &table = get_table();

    for (int i = 0; i < TIMED_BLOCK_CAPACITY; ++i) {
        auto &rec = table._records[i];

        if (is_nil_record(rec)) {
            continue;
        }

        ss << rec.func_pointer << ":" << rec.line;
        tab(ss, 40);

        ss << double(rec.total_time_spent) * 0.000001 << "ms";
        tab(ss, 60);

        ss << uint32_t(rec.times_entered);
        tab(ss, 90);

        for (int i = 0; i < TIMED_BLOCK_COUNT_TO_KEEP; ++i) {
            if (rec.max_time_spent[i] == std::numeric_limits<uint64_t>::max()) {
                break;
            }

            char ms_str[128] = {};

            snprintf(ms_str, sizeof(ms_str), "%.5f", double(rec.max_time_spent[i]) * 1e-6);
            ss << ms_str << " ms";
            if (i != TIMED_BLOCK_COUNT_TO_KEEP - 1) {
                ss << ", ";
            }
        }
        ss << "\n";
    }

    fprintf(f, "%s", c_str(ss));
}

} // namespace timedblock
