**_plexi_**: IEEE802.15.4e TSCH Network Management System
===================================================

**_plexi_** scheduler interface is a CoAP-based mechanism that exposes node resources, e.g. TSCH slotframes and links, link performance metrics, local DoDAG tree, neighbor lists etc to external schedulers.
**_plexi_** was described and evaluated in [Exarchakos, G., Oztelcan, I., Sarakiotis, D., Liotta, A. "plexi: Adaptive re-scheduling web service of time synchronized low-power wireless networks", 2016, JNCA, Elsevier]

## Modules
**_plexi_** consists of five modules:
* **RPL** - every node provides read only access to the local view of the the DoDAG tree (preferred parent and children). CoAP GET and OBSERVE (event-based and periodic) operations are supported. The files of the module are `plexi-rpl.[ch]`.
* **TSCH** - every node provides read and write access to TSCH slotframes and links. CoAP GET, POST and DELETE operations are supported. The files of the module are `plexi-tsch.[ch]`.
* **Neighbor list** - every node provides read only access to MAC neighborhood. CoAP GET and OBSERVE (event-based and periodic) operations are supported. The files of the module are `plexi-neighbors.[ch]`.
* **Link statistics** - every node provides read only access to configurable link performance statistics probes. CoAP GET, POST and DELETE operations on the configuration of the statistics probes are supported. The files of the module are `plexi-link-statistics.[ch]`. The values of the statistics are retrieved via TSCH link and neighbor list resource.
* **Queue statistics** - every node provides read only access to the size of the queue per neighbor. CoAP GET and OBSERVE (event-based and periodic) operations are supported. The files of the module are `plexi-queue-statistics.[ch]`.

## Requirements
The first implementation of plexi interface is done on Contiki OS v3.0. That is, it relies on the coap rest engine present in that version of Contiki. It is also assumed that RPL and/or IEEE802.15.4e-TSCH is enabled in the node.

## Configuration

To use **_plexi_** you need to set a number of C preprocessor flags and/or parameters in the configuration, i.e. header file, of your project.

1. To disable/enable a module use the following flags:
   * Define `PLEXI_WITH_RPL_DAG_RESOURCE` as `0` or `1`, e.g.:
   ```
   #define PLEXI_WITH_RPL_DAG_RESOURCE 1
   ```
   * Define `PLEXI_WITH_TSCH_RESOURCE` as `0` or `1`:
   ```
   #define PLEXI_WITH_TSCH_RESOURCE 1
   ```
   * Define `PLEXI_WITH_NEIGHBOR_RESOURCE` as `0` or `1`, e.g.:
   ```
   #define PLEXI_WITH_NEIGHBOR_RESOURCE 1
   ```
   * Define `PLEXI_WITH_LINK_STATISTICS` as `0` or `1`, e.g.:
   ```
   #define PLEXI_WITH_LINK_STATISTICS 1
   ```
   * Define `PLEXI_WITH_QUEUE_STATISTICS` as `0` or `1`, e.g.:
   ```
   #define PLEXI_WITH_QUEUE_STATISTICS 1
   ```
> To enable link statistics or queue statistics modules, setting `PLEXI_WITH_LINK_STATISTICS` and `PLEXI_WITH_QUEUE_STATISTICS` is not enough. The TSCH module should also be enabled.
2. To modify the periodicity of notifications sent by observed resources to subscribed clients set the following variables:
  * Periodic notifications from RPL resource defaults to 30sec. Define `PLEXI_RPL_UPDATE_INTERVAL` to change it:
  ```
  #define PLEXI_RPL_UPDATE_INTERVAL (10 * CLOCK_SECOND)
  ```
  * Periodic notifications from TSCH link resource defaults to 10sec. Define `PLEXI_LINK_UPDATE_INTERVAL` to change it:
  ```
  #define PLEXI_LINK_UPDATE_INTERVAL (20 * CLOCK_SECOND)
  ```
  * Periodic notifications from neighbor list resource defaults to 10sec. Define `PLEXI_NEIGHBOR_UPDATE_INTERVAL` to change it:
  ```
  #define PLEXI_NEIGHBOR_UPDATE_INTERVAL (20 * CLOCK_SECOND)
  ```
  * Periodic notifications from link statistics resources defaults to 10sec. Define `PLEXI_LINK_STATS_UPDATE_INTERVAL` to change it:
  ```
  #define PLEXI_LINK_STATS_UPDATE_INTERVAL (20 * CLOCK_SECOND)
  ```
  * Periodic notifications from queue statistics resource defaults to 10sec. Define `PLEXI_QUEUE_STATS_UPDATE_INTERVAL` to change it:
  ```
  #define PLEXI_QUEUE_STATS_UPDATE_INTERVAL (20 * CLOCK_SECOND)
  ```
## Usage

To use **_plexi_**, follow the steps below:

1. Create a `project-conf.h` file in your project folder with the configuration of **_plexi_** as detailed above. Include that file in your application.
2. Add it to your makefile `APPS` with `APPS += plexi`.
3. Start **_plexi_** by calling `plexi_init()` from your application, after
including `#include "plexi.h"`.

## CoAP Interface

**_plexi_** comes with a predefined set of URLs for the resources of the modules in `apps/plexi/plexi-interface.h`. You may modify them by overriding those `#define`s.
