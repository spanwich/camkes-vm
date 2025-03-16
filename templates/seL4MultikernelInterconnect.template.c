/*
 * Copyright 2025, Kry10 Pty. Ltd.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*- from 'global-endpoint.template.c' import allocate_cap with context -*/

#include <sel4/sel4.h>
#include <string.h>
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




/*- set nodes = set() -*/
/*- for end in me.parent.from_ends if not end.instance.type.hardware -*/
    /*- do nodes.add(render_state.label_node_map[end.instance.name]) -*/
/*- endfor -*/

/*- for end in me.parent.to_ends if not end.instance.type.hardware -*/
    /*- do nodes.add(render_state.label_node_map[end.instance.name]) -*/
/*- endfor -*/

/*- set multicore = len(nodes) > 1 -*/

/*- if me in me.parent.from_ends -*/
  /*- set index = me.parent.from_ends.index(me) -*/
  /*- set end = 'from' -*/
/*- elif me in me.parent.to_ends -*/
  /*- set index = me.parent.to_ends.index(me) -*/
  /*- set end = 'to' -*/
/*- endif -*/
#include </*? "%s_%s_%s.h" % (me.interface.name, me.parent.type.name, index) ?*/> 

/*- set page_size = macros.get_page_size(dataport_size, options.architecture) -*/
/*- if page_size == 0 -*/
  /*? raise(TemplateError('Error assigning page size. %d must be at least %d and %d aligned' % (dataport, 4096, 4096))) ?*/
/*- endif -*/


/*- set paddr = None -*/
/*- if multicore -*/
  /*- set global_name = '%s_data' % me.parent.name -*/
  /*- if global_name not in render_state.global_obj_space -*/
      /*- set paddr = render_state.curser -*/
      /*- set paddr = macros.ROUND_UP(paddr, page_size) -*/
      /*- do render_state.__setattr__('curser', paddr + dataport_size) -*/
      /*- do render_state.global_obj_space.__setitem__(global_name, paddr) -*/
  /*- else -*/
      /*- set paddr = render_state.global_obj_space[global_name] -*/
  /*- endif -*/

/*- endif -*/

/*- set dataport_symbol_name = "iq_mem_tport_data" -*/

/*- set shmem_symbol_size = dataport_size -*/
/*? macros.shared_buffer_symbol(sym=dataport_symbol_name, shmem_size=shmem_symbol_size, page_size=page_size) ?*/

/*? register_shared_variable('%s_data' % me.parent.name, dataport_symbol_name, dataport_size, frame_size=page_size, perm='RW', cached=true, paddr=paddr) ?*/
/*? macros.dataport_type(me.interface.type) ?*/ * /*? me.interface.name ?*/ =
    (/*? macros.dataport_type(me.interface.type) ?*/ *) & /*? dataport_symbol_name ?*/;

seL4_CPtr /*? me.interface.name ?*/_sgi_map[MQ_NUM_NODES] = {
/*- for end in (me.parent.from_ends + me.parent.to_ends) if not end == me and not end.instance.type.hardware -*/
  /*- do allocate_cap(end, is_reader=False, multicore=multicore) -*/
  /*- set vm_id = configuration[ end.instance.name].get('vm_id', None) -*/
  /*- set notification = pop('notification') -*/
  [/*? vm_id ?*/] = /*? notification ?*/,
/*- endfor -*/
};


void /*? me.interface.name ?*/_broadcast(bool signal_self) {
  for (int i = 0; i < MQ_NUM_NODES; i++) {
    if (!signal_self && i == /*? configuration[me.instance.name].get('vm_id', None) ?*/) {
      continue;
    }
    seL4_Signal(/*? me.interface.name ?*/_sgi_map[i]);

  }
}

seL4_CPtr /*? me.interface.name ?*/_get_signal_cap(int vm_id) {
  ZF_LOGF_IF(vm_id >= MQ_NUM_NODES, "Invalid arg");
  return /*? me.interface.name ?*/_sgi_map[vm_id];
}

/*# We need to get the badge that they will signal us on #*/
/*- do allocate_cap(me, is_reader=True, multicore=multicore) -*/

/*- set (notification, irq) = pop('notification') -*/
/*- set badge = pop('badge') -*/

seL4_CPtr /*? me.interface.name ?*/_notification(void) {
    return /*? notification ?*/;
}

seL4_Word /*? me.interface.name ?*/_notification_badge(void) {
    return /*? badge ?*/;
}

void /*? me.interface.name ?*/_init(void) {
  memset(/*? me.interface.name ?*/, 0, /*? dataport_size ?*/);
}
