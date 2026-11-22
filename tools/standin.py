"""Symbol-phase stand-in declarations (see BUILDING.md).

SDCC lays out a function's stack slots in the address order of its symbol structures, so with an
XP-style heap the layout depends on every declaration compiled before the function, which for the
original GBX sources is unknown. A function compiled with profile 3.3.0-win32-xpheap may be
preceded by one block of never-referenced `extern int` declarations standing in for those unknown
declarations: a comment line and one declaration (eight names per line) immediately before the
definition. The unit may also open with one such block of differently named declarations standing
in for unknown header declarations (owner-approved extension), kept on a single declaration line.
Blocks spell their names through four fixed digit-group macros, so a count of 14,633 takes a few
lines; the preprocessed declaration is the same list of names.
The blocks emit no code, data or storage and earn no credit. Application sources omit them;
the build inserts them using each region's `symbol_phase_standins` and
`symbol_phase_top_standins` counts, then checks their shape before compilation.
"""

import re

COMMENT = "/* Symbol-phase stand-ins (zero credit): never referenced, no code or storage. */"
TOP_COMMENT = ("/* Symbol-phase stand-ins for unknown header declarations (zero credit): never "
               "referenced, no code or storage. */")
PREFIX, TOP_PREFIX = "symbolPhase", "symbolPhaseTop"
MAXIMUM = 99999
PER_LINE = 4

# SYMBOL_PHASE_<10^n>(p) expands to p followed by every n-digit suffix, each with a trailing comma.
# The preprocessor runs as a separate process, so only the expanded declaration reaches the
# compiler. Four definition lines keep the skipped run short enough that the preprocessor emits
# newlines rather than a line marker (each line marker with a file name allocates in the compiler).
MACROS = "".join(
    "#define SYMBOL_PHASE_{}(p) {}\n".format(
        10 ** n, " ".join("p##{},".format(i) if n == 1 else
                          "SYMBOL_PHASE_{}(p##{})".format(10 ** (n - 1), i) for i in range(10)))
    for n in range(1, 5))


def name(index, count, prefix=PREFIX):
    """Stand-in name: zero-padded to three digits, or to the width of the largest index."""
    return "{}{:0{}d}".format(prefix, index, max(3, len(str(count - 1))))


def declarators(count, prefix):
    """Items declaring names 0 .. count - 2 (digit groups through the macros, then single names)
    and the final name: together exactly `count` declarators, as one declaration."""
    width = max(3, len(str(count - 1)))
    digits = "{:0{}d}".format(count - 1, width)
    items = []
    for position, digit in enumerate(digits):
        size = 10 ** (width - position - 1)
        for value in range(int(digit)):
            head = prefix + digits[:position] + str(value)
            items.append(head + "," if size == 1 else "SYMBOL_PHASE_{}({})".format(size, head))
    return items + [name(count - 1, count, prefix) + ";"]


def block(count, top=False):
    """Exact block text for `count` stand-ins (empty for 0); `top` selects the unit-opening block,
    whose declaration stays on one line so later line numbers do not depend on the count."""
    if not 0 <= count <= MAXIMUM:
        raise ValueError("Invalid symbol-phase stand-in count")
    if not count:
        return ""
    items = declarators(count, TOP_PREFIX if top else PREFIX)
    if top:
        return TOP_COMMENT + "\n" + MACROS + "extern int " + " ".join(items) + "\n"
    lines = [" ".join(items[i:i + PER_LINE]) for i in range(0, len(items), PER_LINE)]
    return COMMENT + "\n" + MACROS + "extern int " + "\n    ".join(lines) + "\n"


def parse(candidate, comment, prefix):
    """Count of a block text starting with `comment` (0 if it is not one); rejects malformed ones."""
    match = re.fullmatch(re.escape(comment + "\n" + MACROS) + r"extern int [^;]*?(\w+);\n",
                         candidate)
    if not match:
        return 0
    last = match.group(1)
    if not (last.startswith(prefix) and last[len(prefix):].isdigit()):
        raise ValueError("Malformed symbol-phase stand-in block")
    count = int(last[len(prefix):]) + 1
    if count > MAXIMUM or candidate != block(count, top=prefix == TOP_PREFIX):
        raise ValueError("Malformed symbol-phase stand-in block")
    return count


def insert_top(text, count):
    """Open the unit with the top block."""
    if count_top(text):
        raise ValueError("Unit already opens with symbol-phase stand-ins")
    return block(count, top=True) + text


def count_top(text):
    """Stand-in count of the block opening the unit (0 if none); rejects a malformed one."""
    if not text.startswith(TOP_COMMENT + "\n"):
        return 0
    end = text.find(";\n") + 2
    count = parse(text[:end], TOP_COMMENT, TOP_PREFIX) if end > 1 else 0
    if not count:
        raise ValueError("Malformed symbol-phase stand-in block")
    return count


def insert(text, start, count):
    """Insert the block at the start of the line containing offset `start`."""
    line = text.rfind("\n", 0, start) + 1
    return text[:line] + block(count) + text[line:]


def has_any(text):
    """Whether `text` contains any symbol-phase stand-in identifier or macro."""
    return any(marker in text for marker in ("symbolPhase", "SYMBOL_PHASE"))


def count_before(text, start):
    """Stand-in count of a block ending right before the line containing `start` (0 if none);
    rejects stand-ins anywhere else and malformed blocks."""
    line = text.rfind("\n", 0, start) + 1
    top = len(block(count_top(text), top=True))
    if line < top:
        raise ValueError("Definition inside the symbol-phase stand-in block")
    head = text[top:line]
    count = 0
    at = head.rfind(COMMENT + "\n")
    if at >= 0 and (at == 0 or head[at - 1] == "\n") and head.endswith(";\n"):
        count = parse(head[at:], COMMENT, PREFIX)
        if count:
            head = head[:at]
    if any(has_any(part) for part in (head, text[line:])):
        raise ValueError("Symbol-phase stand-ins are only allowed right before the definition")
    return count
