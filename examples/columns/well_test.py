"""well.decode against the dump taken at the after-tape checkpoint."""

import pathlib

import pytest

import well

DUMP = pathlib.Path(__file__).resolve().parent / "well-after-tape.bin"

# What tapes/title-to-game.yaml leaves on screen: two columns landed on the
# floor, in lanes 0 and 3, and a third falling into lane 3.
FLOOR = {(10, 0): 2, (11, 0): 6, (12, 0): 3,
         (10, 3): 2, (11, 3): 1, (12, 3): 3}


@pytest.fixture(scope="module")
def after_tape():
    return well.decode(DUMP.read_bytes())


def test_the_floor_holds_the_two_landed_columns(after_tape):
    for row in range(well.ROWS):
        for cell in range(well.COLUMNS):
            assert after_tape.rows[row][cell] == FLOOR.get((row, cell),
                                                           well.EMPTY)


def test_the_falling_column_is_over_the_middle_lane(after_tape):
    assert after_tape.falling == well.Falling(3, 0, (2, 2, 1))


def test_it_prints_six_columns_of_thirteen_characters(after_tape):
    printed = str(after_tape).splitlines()
    assert len(printed) == well.ROWS
    assert {len(line) for line in printed} == {well.COLUMNS}
    assert printed[-3:] == ["2..2..", "6..1..", "3..3.."]


def test_a_region_that_is_too_short_is_refused():
    with pytest.raises(ValueError):
        well.decode(DUMP.read_bytes()[:well.RAM_BYTES - 1])
