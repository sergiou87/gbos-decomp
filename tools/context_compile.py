"""Pinned in-repo C compilation contexts; emit only the selected function."""

import hashlib
import json
import re
import shutil
from pathlib import Path

import standin

ROOT = Path(__file__).resolve().parents[1]
LINUX_PROFILE_ID = "3.3.0-linux-x86"
WINE_PROFILE_ID = "3.3.0-win32-xpheap"
TOKENS = re.compile(
    r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|'
    r"'(?:\\.|[^'\\])*'|[A-Za-z_]\w*|->|[^\s]")
IDENTIFIER = re.compile(r"[A-Za-z_]\w*\Z")
SHA256 = re.compile(r"[0-9a-f]{64}\Z")


def load_dependencies():
    return json.loads((ROOT / "config/context-sources.json").read_text())


def dependency_path(bundle, filename, dependencies):
    if (not isinstance(bundle, str) or not re.fullmatch(r"[A-Za-z0-9_-]+", bundle)
            or bundle not in dependencies or not isinstance(filename, str)
            or Path(filename).name != filename or filename in ("", ".", "..")
            or filename not in dependencies[bundle]["files"]):
        raise ValueError("Unknown context input: {}/{}".format(bundle, filename))
    directory = dependencies[bundle].get("directory")
    if (not isinstance(directory, str) or not directory
            or Path(directory).is_absolute() or ".." in Path(directory).parts):
        raise ValueError("Context directory must be repository-relative")
    path = ROOT / directory / filename
    try:
        path.resolve().relative_to(ROOT.resolve())
    except ValueError:
        raise ValueError("Context input must stay within the repository")
    return path


def verify_input(path, expected):
    if not isinstance(expected, str) or not SHA256.fullmatch(expected):
        raise ValueError("Invalid context input SHA-256")
    if not path.is_file():
        raise ValueError("Missing in-repo context input: {}".format(path))
    if hashlib.sha256(path.read_bytes()).hexdigest() != expected:
        raise ValueError("Context input checksum mismatch: {}".format(path))


def validate_context(region):
    context = region["context"]
    compiler = region.get("compiler", "")
    if (region["kind"] != "c" or not isinstance(compiler, str)
            or not (re.fullmatch(r"r[0-9]+", compiler)
                    or compiler in (LINUX_PROFILE_ID, WINE_PROFILE_ID))
            or not isinstance(context, dict)):
        raise ValueError("Compilation context requires a historical C profile")
    required = {"bundle", "source", "function", "replacement", "headers",
                "compiler_flags", "unit_sha256", "bindings"}
    if not required <= context.keys() or context.keys() - required - {"identifiers", "members"}:
        raise ValueError("Invalid compilation context fields")
    if not isinstance(context["unit_sha256"], str) or not SHA256.fullmatch(context["unit_sha256"]):
        raise ValueError("Invalid composed context SHA-256")
    for name in ("function", "replacement"):
        if not isinstance(context[name], str) or not IDENTIFIER.fullmatch(context[name]):
            raise ValueError("Invalid context function name")
    for name in ("headers", "compiler_flags"):
        if (not isinstance(context[name], list)
                or not all(isinstance(item, str) and item for item in context[name])):
            raise ValueError("Context {} must be a list of strings".format(name))
    dependencies = load_dependencies()
    for filename in (context["source"], *context["headers"]):
        dependency_path(context["bundle"], filename, dependencies)
    if len(set(context["headers"])) != len(context["headers"]) or context["source"] in context["headers"]:
        raise ValueError("Duplicate context inputs")
    for name in ("identifiers", "members"):
        mapping = context.get(name, {})
        if (not isinstance(mapping, dict)
                or any(not isinstance(value, str) or not IDENTIFIER.fullmatch(value)
                       for pair in mapping.items() for value in pair)):
            raise ValueError("Context aliases must be C identifiers")
    emitted = context.get("identifiers", {}).get(context["replacement"], context["replacement"])
    if emitted != context["function"]:
        raise ValueError("Context must alias the replacement function")
    if (not isinstance(context["bindings"], dict)
            or any(not re.fullmatch(r"_[A-Za-z_]\w*", name)
                   or not isinstance(value, str) or not re.fullmatch(r"0x[0-9a-fA-F]{1,4}", value)
                   for name, value in context["bindings"].items())):
        raise ValueError("Context bindings must be fixed 16-bit addresses")
    if "_" + context["function"] in context["bindings"]:
        raise ValueError("Cannot bind the emitted context function")


def function_span(text, name):
    """Find one ordinary C definition, ignoring braces in comments and literals."""
    if not IDENTIFIER.fullmatch(name):
        raise ValueError("Invalid C function name: " + name)
    masked = list(text)
    for token in TOKENS.finditer(text):
        if token.group().startswith(("//", "/*", '"', "'")):
            masked[token.start():token.end()] = [
                "\n" if character == "\n" else " " for character in token.group()]
    masked = "".join(masked)
    matches = list(re.finditer(
        r"(?m)(?:^|(?<=[;}]))[ \t]*[A-Za-z_][\w \t*]*\b" + re.escape(name)
        + r"\s*\([^;{}]*\)\s*\{", masked))
    if len(matches) != 1:
        raise ValueError("Expected one C definition of " + name)
    start = matches[0].start()
    cursor = matches[0].end()
    depth = 1
    while cursor < len(masked) and depth:
        depth += (masked[cursor] == "{") - (masked[cursor] == "}")
        cursor += 1
    if depth:
        raise ValueError("Unterminated C function: " + name)
    return start, cursor


def rename_identifiers(text, identifiers, members):
    for mapping in (identifiers, members):
        if any(not IDENTIFIER.fullmatch(value) for pair in mapping.items() for value in pair):
            raise ValueError("Context aliases must be C identifiers")
    previous = None

    def replace(match):
        nonlocal previous
        token = match.group()
        if token.startswith(("//", "/*")):
            return token
        mapping = members if previous in (".", "->") else identifiers
        previous = token
        return mapping.get(token, token)

    return TOKENS.sub(replace, text)


def compose_context(template, source, context):
    start, end = function_span(template, context["function"])
    first, limit = function_span(source, context["replacement"])
    # A symbol-phase stand-in block right before the replacement moves with it (see standin.py).
    standins = standin.count_before(source, first)
    replacement = rename_identifiers(
        source[first:limit], context.get("identifiers", {}), context.get("members", {}))
    function_span(replacement, context["function"])
    result = template[:start] + standin.block(standins) + replacement + template[end:]
    if hashlib.sha256(result.encode()).hexdigest() != context["unit_sha256"]:
        raise ValueError("Composed context checksum mismatch")
    return result


def _referenced_symbols(lines, pattern):
    seen = []
    for line in lines:
        code = line.split(";", 1)[0]
        for symbol in pattern.findall(code):
            if symbol not in seen:
                seen.append(symbol)
    return seen


def function_assembly(text, name):
    lines = text.splitlines()
    starts = [index for index, line in enumerate(lines)
              if re.fullmatch("_" + re.escape(name) + r"::?", line.strip())]
    if len(starts) != 1:
        raise ValueError("Compiler did not emit one function " + name)
    str_ref = re.compile(r"\b(___str_\d+)\b")
    start = starts[0]
    end = start + 1
    while end < len(lines):
        line = lines[end].strip()
        if line.startswith(".area ") or line.startswith(".globl "):
            break
        str_label = re.fullmatch(r"(___str_\d+)::?", line)
        if str_label and str_label.group(1) not in _referenced_symbols(lines[start:end], str_ref):
            break
        if (re.match(r"_[A-Za-z]\w*::?$", line)
                and not re.fullmatch("_" + re.escape(name) + r"_end::?", line)
                and not re.fullmatch("_" + re.escape(name) + r"_\w+_\d+_\d+:", line)):
            break
        end += 1
    extracted = list(lines[start:end])
    defined = {
        match.group(1)
        for line in extracted
        for match in [re.match(r"^([A-Za-z_]\w*)::?", line.strip())]
        if match
    }
    for label in _referenced_symbols(extracted, str_ref):
        if label in defined:
            continue
        matches = [index for index, line in enumerate(lines)
                   if re.fullmatch(re.escape(label) + r"::?", line.strip())]
        if len(matches) != 1:
            raise ValueError("Missing constant block {} referenced by {}".format(label, name))
        block_end = matches[0] + 1
        while block_end < len(lines):
            stripped = lines[block_end].strip()
            if (stripped.startswith(".area ") or stripped.startswith(".globl ")
                    or stripped.startswith(";---") or re.match(r"^[A-Za-z_]\w*::?$", stripped)):
                break
            block_end += 1
        extracted.extend(lines[matches[0]:block_end])
        defined.add(label)
    return "\n".join(extracted) + "\n"


def prepare_context(region, assembly, source_root=None, check_standins=True):
    """Copy verified inputs beside the output and compose the compiled unit."""
    validate_context(region)
    if region.get("symbol_phase_top_standins", 0):
        raise ValueError("Top-of-unit stand-ins require a plain region")
    expected_standins = region.get("symbol_phase_standins", 0)
    if expected_standins and region.get("compiler") != WINE_PROFILE_ID:
        raise ValueError("Symbol-phase stand-ins require profile " + WINE_PROFILE_ID)
    context = region["context"]
    dependencies = load_dependencies()
    bundle = context["bundle"]
    directory = assembly.parent / (assembly.stem + ".context")
    if directory.exists():
        shutil.rmtree(directory)
    directory.mkdir(parents=True)
    names = [context["source"], *context["headers"]]
    for filename in names:
        path = dependency_path(bundle, filename, dependencies)
        verify_input(path, dependencies[bundle]["files"][filename])
        shutil.copyfile(path, directory / filename)
    unit = directory / context["source"]
    # Only the recovered definition is inserted; canonical typedefs/externs are
    # for readers and host tests, not additional compiler allocation history.
    root = ROOT if source_root is None else source_root
    source = (root / region["source"]).read_text()
    if check_standins:
        found_top = standin.count_top(source)
        if found_top:
            raise ValueError("{} opens with {} symbol-phase stand-ins, config records 0".format(
                region["source"], found_top))
    source = standin.insert(source, function_span(source, context["replacement"])[0],
                            expected_standins)
    if check_standins:
        found = standin.count_before(source, function_span(source, context["replacement"])[0])
        if found != expected_standins:
            raise ValueError("{} has {} symbol-phase stand-ins, config records {}".format(
                region["source"], found, expected_standins))
    unit.write_text(compose_context(unit.read_text(), source, context))
    return directory, unit


def context_flags(region):
    optimization = region.get("optimization", "size")
    flags = [] if optimization == "default" else ["--opt-code-" + optimization]
    return [*flags, *region.get("compiler_flags", []), *region["context"]["compiler_flags"]]


def compile_context(region, assembly, compiler, env, run):
    directory, unit = prepare_context(region, assembly)
    command = [compiler, "-mgbz80", *context_flags(region),
               "-I", directory, "-S", unit.name, "-o", assembly]
    run(command, env=env, cwd=directory)
    return wrap_context(region, assembly)


def wrap_context(region, assembly):
    context = region["context"]
    return wrap_function(region, assembly, context["function"], context["bindings"])


def wrap_function(region, assembly, function, bindings):
    raw = assembly.read_text()
    if len(re.findall(r"(?m)^[ \t]*[.]optsdcc -mgbz80[ \t]*$", raw)) != 1:
        raise ValueError("Unexpected historical object ABI metadata")
    section = function_assembly(raw, function)
    defined = {
        match.group(1)
        for line in section.splitlines()
        for match in [re.match(r"^([A-Za-z_]\w*)::?", line.strip())]
        if match
    }
    local_static = re.compile(r"\b(_" + re.escape(function) + r"_\w+_\d+_\d+)\b")
    for symbol in _referenced_symbols(section.splitlines(), local_static):
        if symbol not in defined and symbol not in bindings:
            raise ValueError("Unbound function-local static symbol: " + symbol)
    module_name = "context_" + region.get("name", function)
    if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", module_name):
        raise ValueError("Invalid context module name: " + module_name)
    wrapper = assembly.with_suffix(".sdas.s")
    binding_lines = "".join("{} = {}\n".format(name, address) for name, address in bindings.items())
    # The compiler's assembly is retained intact. The wrapper copies the selected
    # function verbatim into its own relocatable area and binds external symbols.
    wrapper.write_text(".module {}\n\t.optsdcc -msm83 sdcccall(0)\n{}.area _{}\n{}".format(
        module_name, binding_lines, region["area"], section))
    return wrapper
