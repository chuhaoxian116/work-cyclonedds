# 1 kHz arm-state DDS demo

This sample validates a one-way, latest-state data stream:

```text
dds_arm_state_publisher  --->  dds_arm_state_subscriber
          1 ms                       WaitSet
```

It intentionally has no application-level ACK. The publisher records its own
release timing and write count. The subscriber records receive intervals,
absolute jitter from the configured period, sequence gaps, duplicates,
out-of-order samples and DDS status counters.

The current single-threaded demo performs no periodic sorting or logging in
the 1 kHz data path. It collects timing values in memory and prints one final
summary after the measurement loop exits. A production implementation that
needs live monitoring should move aggregation and logging to a separate
non-real-time thread or use a fixed-size histogram.

Before measurement starts, both processes reserve enough statistics storage
for the configured duration and period. This prevents `std::vector` growth
and data relocation from introducing latency spikes in the 1 kHz path.

## QoS profile

```text
Reliability  BestEffort
Durability   Volatile
History      KeepLast(1)
Deadline     2 ms by default
Lifespan     5 ms by default
```

`Deadline` is a periodic-data watchdog. It does not delay delivery. With a
2 ms deadline, the reader reports a missed deadline when no new value arrives
within two expected 1 ms cycles. The writer can likewise report that it failed
to publish within the offered deadline.

`Lifespan` is an age limit for each sample. It also does not delay delivery.
A 5 ms lifespan means a sample delayed for five control cycles is stale and
must not be delivered as current state. Five milliseconds is only a starting
value: production code must derive it from the robot controller's maximum
acceptable state age. Use a smaller value when the controller cannot safely
consume state that old.

`KeepLast(1)` favours freshness over completeness. A slow reader may skip
intermediate states, so the `sequence_` field is required to measure gaps.

## Build

From the `myproject` directory:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --parallel
```

## Run

Arguments for both executables are:

```text
[duration-seconds] [period-us] [deadline-ms] [lifespan-ms]
```

Start the subscriber first:

```bash
CYCLONEDDS_URI=file://"$PWD/config/cyclonedds.xml" \
  ./build/bin/dds_arm_state_subscriber 60 1000 2 5
```

Then start the publisher:

```bash
CYCLONEDDS_URI=file://"$PWD/config/cyclonedds.xml" \
  ./build/bin/dds_arm_state_publisher 60 1000 2 5
```

For the 5211-second test, replace `60` with `5211`. Redirect stdout and stderr
to files for long runs; do not print each sample.

The subscriber's receive interval measures application-visible cadence, not
one-way network latency. Measuring one-way latency requires synchronized clocks
such as PTP and a timestamp domain shared by both hosts.
