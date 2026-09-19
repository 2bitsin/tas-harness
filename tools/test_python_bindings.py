"""The scenario API the built extension exposes, read from the module."""

import importlib.util
import os
import pathlib
import sys

import pytest

MODULE_FUNCTIONS = (
    "exact_hash", "difference_hash", "perceptual_hash", "change_from", "find",
    "watch", "watches", "search",
)

PLAN_FUNCTIONS = ("route", "distances", "graph", "graph_distances")

RUN_METHODS = (
    "step", "run_until", "pace", "hold", "release", "pad", "frames",
    "harness_time", "observe", "look", "shot", "checkpoint", "restore",
    "play",
    "expect", "judge", "mark", "clip", "watch", "bundle", "memory",
    "memory_stable", "colours", "report",
)


def _bridge() -> pathlib.Path | None:
    named = os.environ.get("TASH_BRIDGE_DIR")
    roots = [pathlib.Path(named)] if named else []
    roots += [pathlib.Path(entry).parent
              for entry in os.environ.get("PATH", "").split(os.pathsep)
              if pathlib.Path(entry).name == "bin"]
    for root in roots:
        found = sorted(root.glob("**/tash.cpython-*.so"))
        if found:
            return found[0]
    return None


@pytest.fixture(scope="module")
def tash():
    built = _bridge()
    if built is None:
        pytest.skip("the pybind bridge is not in this build tree "
                    "(no tash.cpython-*.so under the build directory)")
    spec = importlib.util.spec_from_file_location("tash", built)
    module = importlib.util.module_from_spec(spec)
    sys.modules["tash"] = module
    spec.loader.exec_module(module)
    return module


def test_the_module_exposes_the_scenario_functions(tash):
    for name in MODULE_FUNCTIONS:
        assert callable(getattr(tash, name)), name


def test_the_run_type_exposes_the_scenario_methods(tash):
    for name in RUN_METHODS:
        assert callable(getattr(tash.Run, name)), name


def test_an_imported_module_has_no_run(tash):
    assert tash.run is None
    with pytest.raises(RuntimeError):
        tash.exact_hash()


def test_the_plan_submodule_exposes_its_searches(tash):
    for name in PLAN_FUNCTIONS:
        assert callable(getattr(tash.plan, name)), name
    assert tash.plan.IMPASSABLE == 255
    assert tash.plan.UNREACHED > 0


def test_a_route_needs_no_run_and_walks_round_a_wall(tash):
    costs = bytes([1, 255, 1, 1, 255, 1, 1, 1, 1])
    walked = tash.plan.route(costs, 3, 3, (0, 0), (2, 0))

    assert walked["cost"] == 6
    assert walked["cells"][0] == (0, 0) and walked["cells"][-1] == (2, 0)
    assert (1, 0) not in walked["cells"]
    assert tash.plan.route(costs, 3, 3, (0, 0), (2, 0), diagonal=True)[
        "cost"] == 4


def test_distances_spread_from_every_source_at_once(tash):
    costs = bytes([1] * 9)
    spread = tash.plan.distances(costs, 3, 3, [(0, 0), (2, 2)])

    assert spread[0] == 0 and spread[8] == 0
    assert spread[4] == 2
    assert tash.plan.distances(bytes([255] * 9), 3, 3, [(1, 1)]) == [
        tash.plan.UNREACHED] * 9


def test_a_graph_route_walks_the_cheapest_way_by_node_id(tash):
    edges = [(16, 9, 64), (9, 16, 64), (16, 13, 176), (13, 9, 20)]
    walked = tash.plan.graph(edges, 16, 9)

    assert walked["nodes"] == [16, 9]
    assert walked["cost"] == 64
    assert tash.plan.graph(edges, 13, 13) == {"nodes": [13], "cost": 0}


def test_graph_distances_price_every_node_from_every_source(tash):
    edges = [(1, 2, 3), (2, 3, 4), (1, 3, 20)]
    table = tash.plan.graph_distances(edges, [1, 3])

    assert table[1] == {1: 0, 2: 3, 3: 7}
    assert table[3][1] == tash.plan.UNREACHED


def test_a_graph_that_leads_nowhere_is_refused(tash):
    with pytest.raises(RuntimeError):
        tash.plan.graph([(1, 2, 1), (3, 2, 1)], 1, 3)
    with pytest.raises(RuntimeError):
        tash.plan.graph([(1, 2, 1)], 1, 9)
    with pytest.raises(RuntimeError):
        tash.plan.graph_distances([(1, 2, 1)], [9])
    with pytest.raises(RuntimeError):
        tash.plan.graph([(1, 2, -1)], 1, 2)


def test_a_grid_that_is_not_its_own_size_is_refused(tash):
    with pytest.raises(RuntimeError):
        tash.plan.route(bytes([1] * 8), 3, 3, (0, 0), (2, 2))
    with pytest.raises(RuntimeError):
        tash.plan.distances(bytes([1] * 9), 3, 3, [(3, 0)])
