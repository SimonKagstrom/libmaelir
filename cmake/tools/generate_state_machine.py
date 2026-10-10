#!/usr/bin/env python3
#
# Generate a state machine base class from a Mermaid state diagram in a markdown file.
#
# The first ```mermaid block in the file is used, and a subset of stateDiagram(-v2) is
# supported:
#
#   [*] --> Name                   The initial state (exactly one)
#   Name --> Other                 A transition
#   Name --> Other : label         A transition with a label (used as a comment)
#   Name : description             Text in the state (used as a comment)
#   state Name : description       Same as above
#   state "description" as Name    Same as above
#
# Styling (classDef, class, Name:::style), notes, direction and %% comments are ignored.
# Final states (--> [*]), composite states, choice/fork/join and concurrency are rejected.

import argparse
import os
import re
import sys

import jinja2

# Names used by the generated code
RESERVED_NAMES = {
    "GeneratedInitial",
    "StateData",
    "State",
    "Next",
    "CustomData",
    "StayTag",
    "Evaluate",
    "Enter",
    "Exit",
    "OnTransition",
    "RunStateMachine",
    "CurrentState",
    "StateName",
}

IDENTIFIER = r"[A-Za-z_][A-Za-z0-9_]*"
STATE_REF = rf"(\[\*\]|{IDENTIFIER})(?::::{IDENTIFIER})?"

TRANSITION_RE = re.compile(rf"^{STATE_REF}\s*-->\s*{STATE_REF}\s*(?::\s*(.*))?$")
STATE_DESCRIPTION_RE = re.compile(rf"^(?:state\s+)?({IDENTIFIER})(?::::{IDENTIFIER})?\s*:\s*(.*)$")
STATE_AS_RE = re.compile(rf'^state\s+"([^"]*)"\s+as\s+({IDENTIFIER})(?::::{IDENTIFIER})?$')
STATE_DECLARATION_RE = re.compile(rf"^state\s+({IDENTIFIER})(?::::{IDENTIFIER})?$")


class StateMachineError(Exception):
    pass


class State:
    "A state in the diagram"

    def __init__(self, name):
        self.name = name
        self.descriptions = []
        self.transitions = []

    @property
    def description(self):
        return " / ".join(self.descriptions)


class Transition:
    "A transition between two states"

    def __init__(self, target, label):
        self.target = target
        self.label = label


def clean_text(text):
    # Mermaid line breaks
    text = re.sub(r"<br\s*/?>", " ", text, flags=re.IGNORECASE)
    return " ".join(text.split())


def extract_mermaid(markdown):
    "Return the lines of the first mermaid block, with their line numbers"
    lines = markdown.splitlines()
    for start, line in enumerate(lines):
        if line.strip() == "```mermaid":
            for end in range(start + 1, len(lines)):
                if lines[end].strip().startswith("```"):
                    return [(n + 1, lines[n]) for n in range(start + 1, end)]
            raise StateMachineError("The ```mermaid block is not terminated")

    raise StateMachineError("No ```mermaid block found")


def parse(mermaid_lines):
    "Parse the diagram, and return (initial state name, list of states)"
    states = {}
    initial_state = None

    def get_state(name, line_number):
        if name in RESERVED_NAMES:
            raise StateMachineError(f"line {line_number}: '{name}' is a reserved name")
        if name not in states:
            states[name] = State(name)
        return states[name]

    in_note = False
    seen_header = False
    for line_number, line in mermaid_lines:
        line = line.strip()

        if in_note:
            in_note = line != "end note"
            continue
        if not line or line.startswith("%%"):
            continue

        if not seen_header:
            if line not in ("stateDiagram", "stateDiagram-v2"):
                raise StateMachineError(
                    f"line {line_number}: Expected stateDiagram-v2, got '{line}'"
                )
            seen_header = True
            continue

        if line.startswith(("direction ", "classDef ", "class ")):
            continue
        if line.startswith("note "):
            # Multi-line notes end with "end note", single-line notes have a ':'
            in_note = ":" not in line
            continue
        if line == "--" or line.endswith("{") or line == "}":
            raise StateMachineError(
                f"line {line_number}: Composite states and concurrency are not supported"
            )
        if "<<" in line:
            raise StateMachineError(
                f"line {line_number}: Choice, fork and join are not supported"
            )

        if match := TRANSITION_RE.match(line):
            source, target, label = match.groups()
            if target == "[*]":
                raise StateMachineError(
                    f"line {line_number}: Final states (--> [*]) are not supported"
                )
            if source == "[*]":
                if initial_state is not None and initial_state != target:
                    raise StateMachineError(
                        f"line {line_number}: More than one initial state "
                        f"({initial_state} and {target})"
                    )
                initial_state = target
                get_state(target, line_number)
                continue

            get_state(target, line_number)
            get_state(source, line_number).transitions.append(
                Transition(target, clean_text(label) if label else None)
            )
        elif match := STATE_AS_RE.match(line):
            description, name = match.groups()
            get_state(name, line_number).descriptions.append(clean_text(description))
        elif match := STATE_DESCRIPTION_RE.match(line):
            name, description = match.groups()
            get_state(name, line_number).descriptions.append(clean_text(description))
        elif match := STATE_DECLARATION_RE.match(line):
            get_state(match.group(1), line_number)
        else:
            raise StateMachineError(f"line {line_number}: Can't parse '{line}'")

    if not seen_header:
        raise StateMachineError("The mermaid block is empty")
    if initial_state is None:
        raise StateMachineError("No initial state ([*] --> State) found")

    # The initial state first, then in order of appearance
    ordered = [states[initial_state]] + [s for s in states.values() if s.name != initial_state]

    return initial_state, ordered


def warn_unreachable(initial_state, states):
    by_name = {state.name: state for state in states}
    reachable = set()
    pending = [initial_state]
    while pending:
        name = pending.pop()
        if name not in reachable:
            reachable.add(name)
            pending.extend(t.target for t in by_name[name].transitions)

    for state in states:
        if state.name not in reachable:
            print(f"warning: state '{state.name}' is unreachable", file=sys.stderr)


def class_name_from_path(path):
    "wifi_state_machine.md -> WifiStateMachine"
    stem = os.path.splitext(os.path.basename(path))[0]
    return "".join(part.capitalize() for part in re.split(r"[^A-Za-z0-9]+", stem) if part)


def main():
    parser = argparse.ArgumentParser(
        description="Generate a state machine base class from a Mermaid state diagram"
    )
    parser.add_argument("input", help="Markdown file with a ```mermaid stateDiagram block")
    parser.add_argument("output", help="The header file to generate")
    parser.add_argument(
        "--class-name",
        help="Base name of the generated classes (default: from the input file name, "
        "e.g., wifi_state_machine.md -> WifiStateMachine)",
    )
    args = parser.parse_args()

    class_name = args.class_name or class_name_from_path(args.input)

    try:
        with open(args.input) as f:
            initial_state, states = parse(extract_mermaid(f.read()))
    except StateMachineError as e:
        print(f"{args.input}: error: {e}", file=sys.stderr)
        sys.exit(1)

    warn_unreachable(initial_state, states)

    template_directory = os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "..", "templates"
    )
    template_env = jinja2.Environment(loader=jinja2.FileSystemLoader(template_directory))
    template = template_env.get_template("generated_state_machine_base.hh.jinja2")

    output = template.render(
        class_name=class_name,
        source=os.path.basename(args.input),
        initial_state=initial_state,
        states=states,
    )

    output_directory = os.path.dirname(args.output)
    if output_directory:
        os.makedirs(output_directory, exist_ok=True)
    with open(args.output, "w") as f:
        f.write(output)


if __name__ == "__main__":
    main()
