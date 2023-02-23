#include <scaffold/string_id.h>

namespace fo {

namespace internal {

StringIDTable &global_string_id_table() {
    static StringIDTable string_id_table;
    return string_id_table;
}

} // namespace internal

} // namespace fo
