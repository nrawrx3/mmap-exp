#pragma once

#include <scaffold/array.h>
#include <scaffold/memory.h>
#include <scaffold/string_stream.h>

/// Contains a scanner (tokenizer) that suffices for a lot of use-cases. Very
/// similar to Go's standard library's scanner package. Only notable difference
/// is that we don't support 'peeking' i.e. the Next() method in go's package.
/// Our next() function is same as the Scan() method in go's package.
namespace scanner {

/// Each of these represent a class of tokens
const int INT = -1;
const int FLOAT = -2;
const int STRING = -3;
const int IDENT = -4;
const int SPACE = -5;
const int COMMENT = -6;
const int EOFS = -7;
const int ESCAPE = -8;
const int INVALID = -9;

enum Modes {
    SCAN_INTS = 1 << -INT,         // Scan integers?
    SCAN_FLOATS = 1 << -FLOAT,     // Scan floats?
    SCAN_STRINGS = 1 << -STRING,   // Scan string literals (e.g "Hello\n")
    SCAN_IDENTS = 1 << -IDENT,     // Scan identifiers (e.g hello_911)
    SCAN_SPACES = 1 << -SPACE,     // Scan white spaces
    SCAN_COMMENTS = 1 << -COMMENT, // Scan comments (not yet implemented)
    SCAN_ESCAPES = 1 << -ESCAPE,   // This will inteprete escape codes all throughout the string
};

/// This default mode is like scanning most simple languages
const int DEFAULT_MODE = SCAN_INTS | SCAN_FLOATS | SCAN_STRINGS | SCAN_IDENTS;

/// This mode treats the whole text as if being inside a string literal. It
/// scans characters one by one and also interpretes escape codes.
const int WHOLESTRING_MODE = SCAN_SPACES | SCAN_ESCAPES;

using Buffer = fo::string_stream::Buffer;

/// Contains the current state of the scanner. As you see, most of the fields
/// are public.
struct SCAFFOLD_API Scanner {
    mutable Buffer _text; // The text (this is mutable due to using c_str)
    int mode;             // The mode
    int line;             // Contains the line number if the *next* token. Starts with 1.
    int col;              // Contains the column of the current line
    int offset;           // Contains the length in characters of the last scanned token
    int token_start;      // Contains the index of the starting position of the last scanned token
    int current_tok;      // Contains the last token-class that next() returned
    long current_int;     // If next() returns INT, then contains that integer
    double current_float; // If next() returns FLOAT, then contains that float

    Scanner() = default;

    Scanner(Buffer text, int mode = DEFAULT_MODE);

    // @copassmov default
};

void init_from_cstring(Scanner &scanner, const char *cstring, int mode = DEFAULT_MODE);

/// Returns a number, which, if negative, denotes the next token's class. If positive, it is equal to the next
/// character. This happens when none of the classes match.
SCAFFOLD_API int next(Scanner &s);

/// Returns a description of the given token id
SCAFFOLD_API const char *desc(int token);

/// Fills the buffer with the current token text
SCAFFOLD_API void token_text(const Scanner &s, Buffer &b);

/// Overload of token_text that returns a null terminated c-string
SCAFFOLD_API char *token_text(const Scanner &s, fo::Allocator &a);

/// Pushes into `b` all the characters from `raw` while decoding the escape codes in `raw` (e.g - The sequence
/// of characters '\' and 'n' in `raw` will be pushed as a single newline character into `b`.
SCAFFOLD_API void string_token(Buffer &b, Buffer &raw);

} // namespace scanner
