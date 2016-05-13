# UDP Random Traffic Generator

This app generates UDP traffic to a set of IPv6 destinations set by the user by creating a peer-to-peer network.
There is a random interval between two UDP packets. The interval is drawn from a
probability density function of choice. The app also receives the UDP packets destined
to this node.

## Features
* Start the UDP traffic generation with `traffic_init()` implemented in `traffic.[ch]`
* Stop the UDP traffic generation with `traffic_end()` implemented in `traffic.[ch]`
* Wait for incoming UDP packets on the port - default `9011` set in `traffic-conf.h`.
* Define a set of destinations from which a random (uniformly) one is picked for every outgoing UDP packet. Destinations defined by user in own configuration file as an array of strings the name of which should be assigned to the macro `TRAFFIC_DESTINATIONS`.
* The interval between consecutive packets follows a PDF, the CDF of which is defined in `traffic-cdfs.h`.
* Shrink the maximum interval by a power of 2. The maximum interval is 65536 seconds. The shrinking factor is the `x` of the formula `65536/(2^x)`. Set it in your configuration file defining the macro `TRAFFIC_CDF_SHRINK_FACTOR`.
* Provide custom payload of outgoing UDP packets via redefining the appropriate callback `TRAFFIC_TRANSMIT_PAYLOAD`.
* Provide custom payload processing of incoming UDP packets via redefining the appropriate callback `TRAFFIC_RECEIVE_CALLBACK`.

## Code Structure
* `traffic.[ch]`: Implementation of the UDP peer (client+server). It includes:
  * the process to be run
  * the translation of string IPv6 addresses to `uip_ipaddr_t` `struct`s,
  * the definition of the callback that populates the payload of each packet with `hello` strings. The user may redefine that callback via the `TRAFFIC_TRANSMIT_PAYLOAD` macro,
  * the IPv6 destinations. That is an array of strings of any of the following forms (::x, xxxx::x, xx:xx:xx).
* `traffic-cdfs.h`: The CDFs from which the intervals between consecutive packets are drawn. It includes:
  * the arrays of values that represent the CDFs
  * the size of each CDF array
* `traffic-conf.h`: The parameters of the traffic generator. Parameters include:
  * the UDP port for that service (defaults to `9011`),
  * the CDF of choice (e.g. pareto or normal),
  * Optional definition of the `TRAFFIC_TRANSMIT_PAYLOAD` macro for customizing the outgoing UDP payload. It defaults to `traffic_transmit_hello` declared in `traffic.h`.
  * Optional definition of the `TRAFFIC_RECEIVE_CALLBACK` macro for custom processing of incoming UDP packets. No definition is given for this macro.

## Usage
The following are basic indicative steps. If you know what you are doing, of course, you may give different structure to your code. The first six steps are necessary and the following are optional.

### Minimum configuration
Minimum usage pattern uses the default port, the pareto cdf, the `hello` payload for all outgoing packets and no processing of the incoming packets. To start generating traffic follow the steps:

1. Define the name of the array with your destinations in any header file e.g. `project-conf.h` file of your project:
```
#define TRAFFIC_DESTINATIONS sinks
```

2. Define the number of your destinations in the same header file e.g. `project-conf.h` file of your project:
```
#define TRAFFIC_DESTINATIONS_COUNT 1
```

3. Define the array of destinations in a `.c` or `.h` file of your project:
```
static const char *sinks[TRAFFIC_DESTINATIONS_COUNT] = {
  "c30c:0:0:1"
};
```
> Note that you might have different set of destinations for different type of nodes i.e. border router and leaf nodes
> That is, a special type node may also configured not to send traffic but only to receive. This is achieved by simply not defining such array of destinations. See also the [examples/traffic](https://github.com/gexarchakos/contiki/blob/traffic/examples/traffic/README.md)

4. Add to your makefile the app by inserting the line:
```
APPS+=traffic
```

5. Include `traffic.h` in your `.c` file that contains your `PROCESS_THREAD`, i.e.
```
#include "traffic.h"
```

6. Start the traffic generation e.g. in your `PROCESS_THREAD` with:
```
traffic_init();
```

7. Stop the traffic generation e.g. at the end of your `PROCESS_THREAD` with:
```
traffic_end();
```

### Customize outgoing UDP packets
Redefine the `TRAFFIC_TRANSMIT_PAYLOAD` macro with your own callback:

8. Define the same macro to your callback in the same header file e.g. in `project-conf.h` and declare the signature of your callback:
```
#define TRAFFIC_TRANSMIT_PAYLOAD my_awesome_payload
int my_awesome_payload(char* buffer, int max);
```

9. Implement your callback in any file of your choice e.g. `node.c`. Note the signature which can also be found in `traffic.h`. Make sure the header file is `#include`d in the `node.c`. An example:
```
...
#include "project-conf.h"
...
int
my_awesome_payload(char* buffer, int max) {
  char* temp = "i spam";
  int i;
  for(i=0; i<7; i++)
  {
    buffer[i] = temp[i];
  }
  return 6;
}
```

### Process incoming UDP packets
Redefine the `TRAFFIC_RECEIVE_CALLBACK` macro with your own callback:

10. In one of your project's header file e.g. `project-conf.h` define the macro and declare the signature of your callback. You may see the signature also in `traffic.h`:
```
#define TRAFFIC_RECEIVE_CALLBACK received_awesome_payload
void received_awesome_payload(uip_ipaddr_t *srcaddr, uint16_t srcport, char* payload);
```

11. Implement your callback in any file of your choice e.g. `border-router.c`.Make sure the header file is `#include`d in the `border-router.c`. An example:
```
...
#include "project-conf.h"
...
void
received_awesome_payload(uip_ipaddr_t *srcaddr, uint16_t srcport, char* payload) {
  printf("!!! Spamming is not awesome !!!\n");
}
```

### Select CDF for intervals
For the moment there are only two available PDFs in `traffic-cdfs.h`: pareto (default) and normal distributions. To select one of the two distributions do the following:

12. In your project's header file e.g. `project-conf.h` define the selected CDF. The CDFs are represented as arrays of values from 0 to 65536. As these represent seconds, it is most likely that one would like to restrict the maximum value to something smaller. This is done by right shifting the randomly chosen value by a number of bit positions. This number is the `TRAFFIC_CDF_SHRINK_FACTOR`.
```
#define TRAFFIC_CDF STDNORMAL //or PARETO
#define TRAFFIC_CDF_SHRINK_FACTOR 5 //now returned intervals will be between 0 and 2048 seconds
```

### Other customizations

13. Port number for incoming packets is specified in `traffic-conf.h`. You may change it in your projects header file:
```
#define TRAFFIC_PORT 9876
```

For more see the example [examples/traffic](https://github.com/gexarchakos/contiki/tree/traffic/examples/traffic).
