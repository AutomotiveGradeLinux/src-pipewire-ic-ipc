/* PipeWire AGL Cluster IPC
 *
 * Copyright © 2021 Collabora Ltd.
 *    @author Julian Bouzas <julian.bouzas@collabora.com>
 *
 * SPDX-License-Identifier: MIT
 */

#define _GNU_SOURCE
#include "test.h"
#include <icipc.h>
#include <unistd.h>
#include <pthread.h>

struct event_data {
  const uint8_t * expected_data;
  size_t expected_size;
  int connections;
  int n_events;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
};

static inline char *new_address()
{
  char *address = NULL;
  (void) asprintf(&address, "icipc-test-%d-%d", getpid(), rand());
  test_ptr_notnull(address);
  return address;
}

static void
wait_for_event (struct event_data *data, int n_events)
{
  pthread_mutex_lock (&data->mutex);
  while (data->n_events < n_events)
    pthread_cond_wait (&data->cond, &data->mutex);
  pthread_mutex_unlock (&data->mutex);
}

static void
sender_state_callback (struct icipc_receiver *self, int sender_fd,
    enum icipc_receiver_sender_state sender_state, void *p)
{
  struct event_data *data = p;
  test_ptr_notnull (data);

  pthread_mutex_lock (&data->mutex);
  switch (sender_state) {
    case ICIPC_RECEIVER_SENDER_STATE_CONNECTED:
      data->connections++;
      break;
    case ICIPC_RECEIVER_SENDER_STATE_DISCONNECTED:
      data->connections--;
      break;
    default:
      test_fail_if_reached ();
      break;
  }
  data->n_events++;
  pthread_cond_signal (&data->cond);
  pthread_mutex_unlock (&data->mutex);
}

static void
reply_callback (struct icipc_sender *self, const uint8_t *buffer, size_t size, void *p)
{
  struct event_data *data = p;
  test_ptr_notnull (data);
  test_ptr_notnull (buffer);

  pthread_mutex_lock (&data->mutex);
  test_cmpint(size, ==, data->expected_size);
  test_cmpint(memcmp(buffer, data->expected_data, size), ==, 0);
  data->n_events++;
  pthread_cond_signal (&data->cond);
  pthread_mutex_unlock (&data->mutex);
}

static void
test_icipc_receiver_basic ()
{
  char *address = new_address();
  struct icipc_receiver *r = icipc_receiver_new (address, 16, NULL, NULL, 0);
  test_ptr_notnull (r);

  /* start and stop */
  test_bool_false (icipc_receiver_is_running (r));
  test_bool_true (icipc_receiver_start (r));
  test_bool_true (icipc_receiver_is_running (r));
  icipc_receiver_stop (r);
  test_bool_false (icipc_receiver_is_running (r));

  /* clean up */
  icipc_receiver_free (r);
  free(address);
}

static void
test_icipc_sender_basic ()
{
  char *address = new_address();
  struct icipc_sender *s = icipc_sender_new (address, 16, NULL, NULL, 0);
  test_ptr_notnull (s);

  /* clean up */
  icipc_sender_free (s);
  free(address);
}

static void
test_icipc_sender_connect ()
{
  static struct icipc_receiver_events events = {
    .sender_state = sender_state_callback,
    .handle_message = NULL,
  };
  struct event_data data = {0};

  pthread_mutex_init (&data.mutex, NULL);
  pthread_cond_init (&data.cond, NULL);

  char *address = new_address();
  struct icipc_receiver *r = icipc_receiver_new (address, 16, &events, &data, 0);
  test_ptr_notnull (r);
  struct icipc_sender *s = icipc_sender_new (address, 16, NULL, NULL, 0);
  test_ptr_notnull (s);

  /* start receiver */
  test_bool_true (icipc_receiver_start (r));

  /* connect sender */
  test_bool_true (icipc_sender_connect (s));
  test_bool_true (icipc_sender_is_connected (s));
  wait_for_event (&data, 1);
  test_cmpint (data.connections, ==, 1);

  /* disconnect sender */
  icipc_sender_disconnect (s);
  test_bool_false (icipc_sender_is_connected (s));
  wait_for_event (&data, 2);
  test_cmpint (data.connections, ==, 0);

  /* stop receiver */
  icipc_receiver_stop (r);

  /* clean up */
  pthread_cond_destroy (&data.cond);
  pthread_mutex_destroy (&data.mutex);
  icipc_sender_free (s);
  icipc_receiver_free (r);
  free(address);
}

static void
lost_connection_handler (struct icipc_sender *self, int receiver_fd, void *p)
{
  struct event_data *data = p;
  test_ptr_notnull (data);

  pthread_mutex_lock (&data->mutex);
  data->n_events++;
  pthread_cond_signal (&data->cond);
  pthread_mutex_unlock (&data->mutex);
}

static void
test_icipc_sender_lost_connection ()
{
  struct event_data data = {0};
  pthread_mutex_init (&data.mutex, NULL);
  pthread_cond_init (&data.cond, NULL);

  char *address = new_address();
  struct icipc_receiver *r = icipc_receiver_new (address, 16, NULL, NULL, 0);
  test_ptr_notnull (r);
  struct icipc_sender *s = icipc_sender_new (address, 16, lost_connection_handler, &data, 0);
  test_ptr_notnull (s);

  /* connect sender */
  test_bool_true (icipc_sender_connect (s));
  test_bool_true (icipc_sender_is_connected (s));

  /* destroy receiver and make sure the lost connection handler is triggered */
  data.n_events = 0;
  icipc_receiver_free (r);
  wait_for_event (&data, 1);

  /* make sure the connection was lost */
  test_bool_false (icipc_sender_is_connected (s));

  /* create a new receiver */
  struct icipc_receiver *r2 = icipc_receiver_new (address, 16, NULL, NULL, 0);
  test_ptr_notnull (r2);

  /* re-connect sender with new receiver */
  test_bool_true (icipc_sender_connect (s));
  test_bool_true (icipc_sender_is_connected (s));

  /* clean up */
  pthread_cond_destroy (&data.cond);
  pthread_mutex_destroy (&data.mutex);
  icipc_sender_free (s);
  icipc_receiver_free (r2);
  free(address);
}

static void
test_icipc_sender_send ()
{
  char *address = new_address();
  struct icipc_receiver *r = icipc_receiver_new (address, 2, NULL, NULL, 0);
  test_ptr_notnull (r);
  struct icipc_sender *s = icipc_sender_new (address, 2, NULL, NULL, 0);
  test_ptr_notnull (s);
  struct event_data data = {0};
  pthread_mutex_init (&data.mutex, NULL);
  pthread_cond_init (&data.cond, NULL);

  /* start receiver */
  test_bool_true (icipc_receiver_start (r));

  /* connect */
  test_bool_true (icipc_sender_connect (s));
  test_bool_true (icipc_sender_is_connected (s));

  /* send 1 byte message (should not realloc) */
  data.n_events = 0;
  data.expected_data = (const uint8_t *)"h";
  data.expected_size = 1;
  test_bool_true (icipc_sender_send (s, (const uint8_t *)"h", 1, reply_callback, &data));
  wait_for_event (&data, 1);

  /* send 2 bytes message (should realloc once to 4) */
  data.n_events = 0;
  data.expected_data = (const uint8_t *)"hi";
  data.expected_size = 2;
  test_bool_true (icipc_sender_send (s, (const uint8_t *)"hi", 2, reply_callback, &data));
  wait_for_event (&data, 1);

  /* send 3 bytes message (should not realloc) */
  data.n_events = 0;
  data.expected_data = (const uint8_t *)"hii";
  data.expected_size = 3;
  test_bool_true (icipc_sender_send (s, (const uint8_t *)"hii", 3, reply_callback, &data));
  wait_for_event (&data, 1);

  /* send 28 bytes message (should realloc 3 times: first to 8, then to 16 and finally to 32) */
  data.n_events = 0;
  data.expected_data = (const uint8_t *)"bigger than 16 bytes message";
  data.expected_size = 28;
  test_bool_true (icipc_sender_send (s, (const uint8_t *)"bigger than 16 bytes message", 28, reply_callback, &data));
  wait_for_event (&data, 1);

  /* don't allow empty messages */
  data.n_events = 0;
  test_bool_false (icipc_sender_send (s, (const uint8_t *)"", 0, NULL, NULL));

  /* stop receiver */
  icipc_receiver_stop (r);

  /* clean up */
  pthread_cond_destroy (&data.cond);
  pthread_mutex_destroy (&data.mutex);
  icipc_sender_free (s);
  icipc_receiver_free (r);
  free(address);
}

static void
test_icipc_multiple_senders_send ()
{
  char *address = new_address();
  struct icipc_receiver *r = icipc_receiver_new (address, 16, NULL, NULL, 0);
  test_ptr_notnull (r);
  struct icipc_sender *senders[50];
  struct event_data data;
  pthread_mutex_init (&data.mutex, NULL);
  pthread_cond_init (&data.cond, NULL);
  data.n_events = 0;

  /* start receiver */
  test_bool_true (icipc_receiver_start (r));

  /* create and connect 50 senders */
  for (int i = 0; i < 50; i++) {
    senders[i] = icipc_sender_new (address, 16, NULL, NULL, 0);
    test_ptr_notnull (senders[i]);
    test_bool_true (icipc_sender_connect (senders[i]));
    test_bool_true (icipc_sender_is_connected (senders[i]));
  }

  /* send 50 messages (1 per sender) */
  data.n_events = 0;
  data.expected_data = (const uint8_t *)"hello";
  data.expected_size = 5;
  for (int i = 0; i < 50; i++)
    test_bool_true (icipc_sender_send (senders[i], (const uint8_t *)"hello", 5, reply_callback, &data));
  wait_for_event (&data, 50);

  /* stop receiver */
  icipc_receiver_stop (r);

  /* clean up */
  pthread_cond_destroy (&data.cond);
  pthread_mutex_destroy (&data.mutex);
  for (int i = 0; i < 50; i++)
    icipc_sender_free (senders[i]);
  icipc_receiver_free (r);
  free(address);
}

int
main (int argc, char *argv[])
{
  test_icipc_receiver_basic();
  test_icipc_sender_basic();
  test_icipc_sender_connect();
  test_icipc_sender_lost_connection();
  test_icipc_sender_send();
  test_icipc_multiple_senders_send();
  return TEST_PASS;
}
