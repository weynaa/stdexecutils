# Various utilities for Senders

Some useful utilities for P2300 Senders
 - `spawn_stdfuture`: eagerly execute a sender and recieve the result in an std::future
 - `QThreadScheduler`: a scheduler that's reusing the a Qt event loop. Includes simple schedule, as well as schedule_at and schedule_after and supports cancellation.

TODO/Ideas:
 - `QSignalSender` only partially useful i guess because you cannot trigger the same sender repeadetly whenever its invoked (maybe a connection would be nicer)
 - `QSlotInvoker` this is only a simple function call, nothing fancy
 - Future type thats QML-compatible: could be useful for passing a std::future/sender like result into QML to have something similar to JS promises with .then()
 -
