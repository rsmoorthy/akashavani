from __future__ import annotations

import json
from pathlib import Path

import pytest
from jsonschema import Draft202012Validator

REPO_ROOT = Path(__file__).resolve().parents[2]
SCHEMA_DIR = REPO_ROOT / "protocol" / "schema"
EXAMPLE_DIR = REPO_ROOT / "protocol" / "examples"

CASES = [
    ("observation.schema.json", "observation.json"),
    ("command.schema.json", "command.json"),
    ("command.schema.json", "command-download-fw.json"),
    ("command-ack.schema.json", "command-ack.json"),
    ("firmware-check.schema.json", "firmware-check.json"),
    ("archive.schema.json", "archive.json"),
]


@pytest.mark.parametrize(("schema_name", "example_name"), CASES)
def test_protocol_examples_match_schemas(schema_name, example_name):
    schema = json.loads((SCHEMA_DIR / schema_name).read_text())
    Draft202012Validator.check_schema(schema)
    example = json.loads((EXAMPLE_DIR / example_name).read_text())
    Draft202012Validator(schema).validate(example)


def test_schema_ids_are_unique_and_versioned():
    ids = []
    for path in SCHEMA_DIR.glob("*.json"):
        schema = json.loads(path.read_text())
        ids.append(schema["$id"])
        assert schema["$schema"].endswith("2020-12/schema")
    assert len(ids) == len(set(ids))
