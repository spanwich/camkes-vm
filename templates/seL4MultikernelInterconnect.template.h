/*
 * Copyright 2025, Kry10 Pty. Ltd.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

// This module should only be responsible for establishing the communication transports.
// Multikernel VMs have a distributed VMM manager architecture where each guest core has
// its own VMM that supervises the virtual core.
// In order to support SMP guests, there are times when the supervisor state needs to be
// synchronized across all cores, or for operations to be correctly serialized by performing
// them on a single core.
//
// This connector type defines something like an interconnect, a way for cores to send and
// recieve messages between each other with reasonable performance.
// While we're targeting low-ish core counts, we are going to attempt a point-point style
// interconnect where each core can independently send and receive messages to all other
// cores directly.
//
// In this configuration, broadcast or multicast operations are achieved by the sender sending
// multiple point-to-point messages to all remotes it wants to communicate with.
//
// We are also going to assume some tight-coupling of the code and interfaces of all communicating
// agents. At this stage they should all be executing the same program. Because of this,
// we will allow them to define the protocol for the messaging language.
//
// We will provide allocation and partitioning of resources to create the transport hardware channels.
//
// For now we will also provide support for sending ipi messages to oneself.
//


// 
// N-to-N topology. For N hw-threads, they can send and receive IPI messages to all other threads in their network.
// Generating signals: N threads can each generate a signal to N-1 threads.
// Receiving signals: N threads can each receive a signal from N-1 threads.
// Leveraging hardware SGI management, only one IRQ line will be used for each hw-thread.
//
// Each client will have a single incoming interrupt line. (Assigned to its global notification under a single badge)
// Each client will have N outgoing signal capabilities including a loop back signal that delivers to itself.
// A region of shared memory will be used to facilite most message passing.
// Each client will have N incoming message queues and N outgoing message queues. There would be N*N queues in total.
// A message queue is expected to operate as a ring buffer of fixed size entries. It should be fine to support linking
// entries at the protocol level.
// For synchonizing on the state of packets in the network, each queue will have a send counter and a receive counter.
// Because each queue has a single sender, this interface doesn't concern itself with synchonization on insertion into the queue,
// but its still necessary to ensure that the counters are used to indicate when queue entries are valid, and atomics should
// be used to ensure that access to the counters are correctly synchronized.
// A queue entry will be the size of a cacheline. On 64-bit, cortex-a53 systems this is 64 bytes.
// For now, only a single instance of this connector can be used with a component.

// For 4 cores, there'd be 16 channels, 1kib mem is needed for each increment in queue size.

/*# These need to be kept in sync with the c file #*/
/*- set mq_num_nodes = configuration[me.parent.name].get('num_nodes') -*/
/*- if mq_num_nodes == None -*/
  /*? raise(TemplateError('%s.num_nodes is not set and must be provided.' % (me.parent.name))) ?*/
/*- endif -*/

/*- set mq_queue_len = configuration[me.parent.name].get('queue_len', 0x40) -*/
/*- set cache_line_size = 64 -*/
/*- set counter_size = 4 -*/
/*- set iq_mem_tport_size = (2 * cache_line_size * mq_num_nodes * mq_num_nodes) + (mq_queue_len * cache_line_size * mq_num_nodes * mq_num_nodes) -*/
/*- set additional_size = configuration[me.parent.name].get('extra_len', 0x0) -*/
/*- set additional_size = macros.ROUND_UP(additional_size, 4096) -*/
/*- set iq_size = macros.ROUND_UP(iq_mem_tport_size, 4096) -*/
/*- set dataport_size = iq_size + additional_size -*/
#include <kernel/gen_config.h>
#include <utils/attribute.h>
#include <utils/arith.h>
#include <camkes/dataport.h>

#define L1_CACHE_LINE_SIZE_BITS CONFIG_L1_CACHE_LINE_SIZE_BITS
#define L1_CACHE_LINE_SIZE BIT(L1_CACHE_LINE_SIZE_BITS)

#define MQ_QUEUE_LEN /*? mq_queue_len ?*/
#define MQ_NUM_NODES /*? mq_num_nodes ?*/
#define IQ_HAVE_INTERFACE 1
#define IQ_ADDITIONAL_SIZE /*? additional_size ?*/

struct iq_mq_buf {
    char iq_mq_buf[L1_CACHE_LINE_SIZE];
}ALIGN(L1_CACHE_LINE_SIZE);

struct iq_mq_queue {
    struct iq_mq_buf mq_queue[MQ_QUEUE_LEN];
} ALIGN(L1_CACHE_LINE_SIZE);

struct iq_mq_counter {
    _Atomic uint32_t iq_mq_counter;
} ALIGN(L1_CACHE_LINE_SIZE);

struct iq_node_counters {
    struct iq_mq_counter iq_node_counters_rx[MQ_NUM_NODES];
    struct iq_mq_counter iq_node_counters_tx[MQ_NUM_NODES];
} ALIGN(L1_CACHE_LINE_SIZE);


struct iq_node_queues {
    struct iq_mq_queue iq_node_queues[MQ_NUM_NODES];
} ALIGN(L1_CACHE_LINE_SIZE);

struct iq_mem_tport {
    // Put all the counters first
    struct iq_node_counters iq_node_counters[MQ_NUM_NODES];
    // Then put all the queues
    struct iq_node_queues iq_node_queues[MQ_NUM_NODES];
} ALIGN(L1_CACHE_LINE_SIZE);

static_assert(L1_CACHE_LINE_SIZE == /*? cache_line_size ?*/, "L1_CACHE_LINE_SIZE not equal to /*? cache_line_size ?*/");
static_assert(sizeof(uint32_t) <= /*? cache_line_size ?*/, "sizeof(uint32_t) not <= to /*? cache_line_size ?*/");
static_assert(sizeof(struct iq_mem_tport) <= /*? dataport_size ?*/, "sizeof(struct iq_mem_tport) not <= to /*? dataport_size ?*/");

void /*? me.interface.name ?*/_broadcast(bool signal_self);

seL4_CPtr /*? me.interface.name ?*/_get_signal_cap(int vm_id);

seL4_CPtr /*? me.interface.name ?*/_notification(void);

seL4_Word /*? me.interface.name ?*/_notification_badge(void);
void /*? me.interface.name ?*/_init(void);


#define iq_node_counter_tx(p_port, src_id, dst_id) (p_port)->iq_node_counters[(dst_id)].iq_node_counters_tx[(src_id)].iq_mq_counter
#define iq_node_counter_rx(p_port, src_id, dst_id) (p_port)->iq_node_counters[(dst_id)].iq_node_counters_rx[(src_id)].iq_mq_counter
#define iq_node_queue_slot(p_port, src_id, dst_id, idx) (p_port)->iq_node_queues[(dst_id)].iq_node_queues[(src_id)].mq_queue[(idx)]

static inline void /*? me.interface.name ?*/_enqueue_msg(struct iq_mem_tport *p_port, int src_id, int dst_id, struct iq_mq_buf *p_msg) {
  while (iq_node_counter_tx(p_port, src_id, dst_id) - iq_node_counter_rx(p_port, src_id, dst_id) >= MQ_QUEUE_LEN) {
    // Spin uintil there's space in the queue.
  }
  int cnt = iq_node_counter_tx(p_port, src_id, dst_id);
  iq_node_queue_slot(p_port, src_id, dst_id, cnt%MQ_QUEUE_LEN) = *p_msg;
  int i = iq_node_counter_tx(p_port, src_id, dst_id);
  iq_node_counter_tx(p_port, src_id, dst_id) = i + 1;
}

#define iq_node_foreach_queue(p_port, dst_id, iter, queue) for \
  ((iter) = 0, (queue) = &(p_port)->iq_node_queues[(dst_id)].iq_node_queues[(iter)]; \
  (iter) < MQ_NUM_NODES; \
  (iter)++, (queue) = &(p_port)->iq_node_queues[(dst_id)].iq_node_queues[(iter)])

#define iq_mq_queue_while_msg(p_port, src_id, dst_id)   while \
  (iq_node_counter_tx(p_port, src_id, dst_id) - iq_node_counter_rx(p_port, src_id, dst_id) > 0)

