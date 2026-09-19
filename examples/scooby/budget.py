"""What a search may spend: expansions and seconds, counted together."""

import time


class Budget:
    """A bound a search carries, so no run is bounded by the wall clock."""

    def __init__(self, expansions, seconds):
        self._expansions = expansions
        self._until = time.monotonic() + seconds
        self._spent = 0

    def tick(self):
        self._spent += 1

    def spent(self):
        return (self._spent >= self._expansions
                or time.monotonic() >= self._until)

    def ticks(self):
        return self._spent

    def left(self):
        return max(0.0, self._until - time.monotonic())
