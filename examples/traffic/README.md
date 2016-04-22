# Example for UDP Traffic Generator app

The example builds a UDP peer-to-peer network. The network uses RPL for routing.
All nodes, except for the border router, are transmitting a UDP packet to the border router.
However, all nodes are listening for incoming UDP packets from the traffic generator app.

The default values of the `traffic` app are used.
* the CDF for time intervals between consecutive packets is STDNORMAL.
* the maximum interval is 2048 seconds (0 is the minimum)
* the UDP port used for incoming packets is 9011

The extra code to integrate traffic app to the example is in the following files:
* In `border-router/Makefile` the line:
```
APPS+=traffic
```
* In `border-router/border-router.c` the lines:
```
#include "traffic.h"
...
traffic_init();
...
traffic_end();
...
```

> Note here that border-router is not supposed to generate traffic but only to receive. Hence, destinations array is not defined.

* In `node/Makefile` the line:
```
APPS+=traffic
```
* In `node/project-conf.h` the lines:
```
#define TRAFFIC_DESTINATIONS sinks
#define TRAFFIC_DESTINATIONS_COUNT 1
#define TRAFFIC_TRANSMIT_PAYLOAD my_awesome_payload
int my_awesome_payload(char* buffer, int max);
```
* In `node/node.c` the lines:
```
#include "traffic.h"
...
const char *sinks[TRAFFIC_DESTINATIONS_COUNT] = {
  "c30c:0:0:1"
};
...
traffic_init();
...
traffic_end();
...
```

For more see the app `traffic`.
