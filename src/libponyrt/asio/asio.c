#define PONY_WANT_ATOMIC_DEFS

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>

#include "asio.h"
#include "../mem/pool.h"

struct asio_base_t
{
  pony_thread_id_t tid;
  asio_backend_t* backend;
  PONY_ATOMIC(uint64_t) noisy_count;
};

static asio_base_t running_base;
static uint32_t asio_cpu;

/** Start an asynchronous I/O event mechanism.
 *
 *  Errors are always delegated to the owning actor of an I/O subscription and
 *  never handled within the runtime system.
 *
 *  In any case (independent of the underlying backend) only one I/O dispatcher
 *  thread will be started. Since I/O events are subscribed by actors, we do
 *  not need to maintain a thread pool. Instead, I/O is processed in the
 *  context of the owning actor.
 */
asio_backend_t* ponyint_asio_get_backend()
{
  return running_base.backend;
}

uint32_t ponyint_asio_get_cpu()
{
  return asio_cpu;
}

void ponyint_asio_init(uint32_t cpu)
{
  asio_cpu = cpu;
  running_base.backend = ponyint_asio_backend_init();
}

bool ponyint_asio_start()
{
  // if the backend wasn't successfully initialized
  if(running_base.backend == NULL)
    return false;

  if(!ponyint_thread_create(&running_base.tid, ponyint_asio_backend_dispatch,
    asio_cpu, running_base.backend))
    return false;

  return true;
}

bool ponyint_asio_stop()
{
  if(!ponyint_asio_stoppable())
    return false;

  if(running_base.backend != NULL)
  {
    ponyint_asio_backend_final(running_base.backend);
    ponyint_thread_join(running_base.tid);

    running_base.backend = NULL;
    running_base.tid = 0;
  }

  return true;
}

bool ponyint_asio_stoppable()
{
  // can only stop if we have no noisy actors
  return atomic_load_explicit(&running_base.noisy_count, memory_order_acquire) == 0;
}

uint64_t ponyint_asio_noisy_add()
{
  return atomic_fetch_add_explicit(&running_base.noisy_count, 1, memory_order_release);
}

uint64_t ponyint_asio_noisy_remove()
{
  return atomic_fetch_sub_explicit(&running_base.noisy_count, 1, memory_order_release);
}
