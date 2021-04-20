/* PipeWire AGL Cluster IPC
 *
 * Copyright © 2021 Collabora Ltd.
 *    @author Julian Bouzas <julian.bouzas@collabora.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include <glib.h>
#include <icipc/icipc.h>

#define TEST_ADDRESS "/tmp/icipc-sender-receiver"

struct event_data {
  const uint8_t * expected_data;
  size_t expected_size;
  int connections;
  int n_events;
  GMutex mutex;
};

static void
wait_for_event (struct event_data *data, int n_events)
{
  while (true) {
    g_mutex_lock (&data->mutex);
    if (data->n_events == n_events) {
      g_mutex_unlock (&data->mutex);
      break;
    }
    g_mutex_unlock (&data->mutex);
  }
}

static void
sender_state_callback (struct icipc_receiver *self, int sender_fd,
    enum icipc_receiver_sender_state sender_state, void *p)
{
  struct event_data *data = p;
  g_assert_nonnull (data);

  g_mutex_lock (&data->mutex);
  switch (sender_state) {
    case ICIPC_RECEIVER_SENDER_STATE_CONNECTED:
      data->connections++;
      break;
    case ICIPC_RECEIVER_SENDER_STATE_DISCONNECTED:
      data->connections--;
      break;
    default:
      g_assert_not_reached ();
      break;
  }
  data->n_events++;
  g_mutex_unlock (&data->mutex);
}

static void
reply_callback (struct icipc_sender *self, const uint8_t *buffer, size_t size, void *p)
{
  struct event_data *data = p;
  g_assert_nonnull (data);
  g_assert_nonnull (buffer);

  g_mutex_lock (&data->mutex);
  g_assert_cmpmem (buffer, size, data->expected_data, data->expected_size);
  data->n_events++;
  g_mutex_unlock (&data->mutex);
}

static void
test_icipc_receiver_basic ()
{
  struct icipc_receiver *r = icipc_receiver_new (TEST_ADDRESS, 16, NULL, NULL, 0);
  g_assert_nonnull (r);

  /* start and stop */
  g_assert_false (icipc_receiver_is_running (r));
  g_assert_true (icipc_receiver_start (r));
  g_assert_true (icipc_receiver_is_running (r));
  icipc_receiver_stop (r);
  g_assert_false (icipc_receiver_is_running (r));

  /* clean up */
  icipc_receiver_free (r);
}

static void
test_icipc_sender_basic ()
{
  struct icipc_sender *s = icipc_sender_new (TEST_ADDRESS, 16, NULL, NULL, 0);
  g_assert_nonnull (s);

  /* clean up */
  icipc_sender_free (s);
}

static void
test_icipc_sender_connect ()
{
  static struct icipc_receiver_events events = {
    .sender_state = sender_state_callback,
    .handle_message = NULL,
  };
  struct event_data data;
  g_mutex_init (&data.mutex);
  data.n_events = 0;
  data.connections = 0;
  struct icipc_receiver *r = icipc_receiver_new (TEST_ADDRESS, 16, &events, &data, 0);
  g_assert_nonnull (r);
  struct icipc_sender *s = icipc_sender_new (TEST_ADDRESS, 16, NULL, NULL, 0);
  g_assert_nonnull (s);

  /* start receiver */
  g_assert_true (icipc_receiver_start (r));

  /* connect sender */
  g_assert_true (icipc_sender_connect (s));
  g_assert_true (icipc_sender_is_connected (s));
  wait_for_event (&data, 1);
  g_assert_cmpint (data.connections, ==, 1);

  /* disconnect sender */
  icipc_sender_disconnect (s);
  g_assert_false (icipc_sender_is_connected (s));
  wait_for_event (&data, 2);
  g_assert_cmpint (data.connections, ==, 0);

  /* stop receiver */
  icipc_receiver_stop (r);

  /* clean up */
  g_mutex_clear (&data.mutex);
  icipc_sender_free (s);
  icipc_receiver_free (r);
}

static void
lost_connection_handler (struct icipc_sender *self, int receiver_fd, void *p)
{
  struct event_data *data = p;
  g_assert_nonnull (data);

  g_mutex_lock (&data->mutex);
  data->n_events++;
  g_mutex_unlock (&data->mutex);
}

static void
test_icipc_sender_lost_connection ()
{
  struct event_data data;
  g_mutex_init (&data.mutex);
  struct icipc_receiver *r = icipc_receiver_new (TEST_ADDRESS, 16, NULL, NULL, 0);
  g_assert_nonnull (r);
  struct icipc_sender *s = icipc_sender_new (TEST_ADDRESS, 16, lost_connection_handler, &data, 0);
  g_assert_nonnull (s);

  /* connect sender */
  g_assert_true (icipc_sender_connect (s));
  g_assert_true (icipc_sender_is_connected (s));

  /* destroy receiver and make sure the lost connection handler is triggered */
  data.n_events = 0;
  icipc_receiver_free (r);
  wait_for_event (&data, 1);

  /* clean up */
  g_mutex_clear (&data.mutex);
  icipc_sender_free (s);
}

static void
test_icipc_sender_send ()
{
  struct icipc_receiver *r = icipc_receiver_new (TEST_ADDRESS, 2, NULL, NULL, 0);
  g_assert_nonnull (r);
  struct icipc_sender *s = icipc_sender_new (TEST_ADDRESS, 2, NULL, NULL, 0);
  g_assert_nonnull (s);
  struct event_data data;
  g_mutex_init (&data.mutex);
  data.n_events = 0;

  /* start receiver */
  g_assert_true (icipc_receiver_start (r));

  /* connect */
  g_assert_true (icipc_sender_connect (s));
  g_assert_true (icipc_sender_is_connected (s));

  /* send 1 byte message (should not realloc) */
  data.n_events = 0;
  data.expected_data = (const uint8_t *)"h";
  data.expected_size = 1;
  g_assert_true (icipc_sender_send (s, (const uint8_t *)"h1", 1, reply_callback, &data));
  wait_for_event (&data, 1);

  /* send 2 bytes message (should realloc once to 4) */
  data.n_events = 0;
  data.expected_data = (const uint8_t *)"hi";
  data.expected_size = 2;
  g_assert_true (icipc_sender_send (s, (const uint8_t *)"hi", 2, reply_callback, &data));
  wait_for_event (&data, 1);

  /* send 3 bytes message (should not realloc) */
  data.n_events = 0;
  data.expected_data = (const uint8_t *)"hii";
  data.expected_size = 3;
  g_assert_true (icipc_sender_send (s, (const uint8_t *)"hii", 3, reply_callback, &data));
  wait_for_event (&data, 1);

  /* send 28 bytes message (should realloc 3 times: first to 8, then to 16 and finally to 32) */
  data.n_events = 0;
  data.expected_data = (const uint8_t *)"bigger than 16 bytes message";
  data.expected_size = 28;
  g_assert_true (icipc_sender_send (s, (const uint8_t *)"bigger than 16 bytes message", 28, reply_callback, &data));
  wait_for_event (&data, 1);

  /* don't allow empty messages */
  data.n_events = 0;
  g_assert_false (icipc_sender_send (s, (const uint8_t *)"", 0, NULL, NULL));

  /* stop receiver */
  icipc_receiver_stop (r);

  /* clean up */
  g_mutex_clear (&data.mutex);
  icipc_sender_free (s);
  icipc_receiver_free (r);
}

static void
test_icipc_multiple_senders_send ()
{
  struct icipc_receiver *r = icipc_receiver_new (TEST_ADDRESS, 16, NULL, NULL, 0);
  g_assert_nonnull (r);
  struct icipc_sender *senders[50];
  struct event_data data;
  g_mutex_init (&data.mutex);
  data.n_events = 0;

  /* start receiver */
  g_assert_true (icipc_receiver_start (r));

  /* create and connect 50 senders */
  for (int i = 0; i < 50; i++) {
    senders[i] = icipc_sender_new (TEST_ADDRESS, 16, NULL, NULL, 0);
    g_assert_nonnull (senders[i]);
    g_assert_true (icipc_sender_connect (senders[i]));
    g_assert_true (icipc_sender_is_connected (senders[i]));
  }

  /* send 50 messages (1 per sender) */
  data.n_events = 0;
  data.expected_data = (const uint8_t *)"hello";
  data.expected_size = 5;
  for (int i = 0; i < 50; i++)
    g_assert_true (icipc_sender_send (senders[i], (const uint8_t *)"hello", 5, reply_callback, &data));
  wait_for_event (&data, 50);

  /* stop receiver */
  icipc_receiver_stop (r);

  /* clean up */
  g_mutex_clear (&data.mutex);
  for (int i = 0; i < 50; i++)
    icipc_sender_free (senders[i]);
  icipc_receiver_free (r);
}

gint
main (gint argc, gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/icipc/receiver-basic", test_icipc_receiver_basic);
  g_test_add_func ("/icipc/sender-basic", test_icipc_sender_basic);
  g_test_add_func ("/icipc/sender-connect", test_icipc_sender_connect);
  g_test_add_func ("/icipc/sender-lost-connection",
      test_icipc_sender_lost_connection);
  g_test_add_func ("/icipc/sender-send", test_icipc_sender_send);
  g_test_add_func ("/icipc/multiple-senders-send",
      test_icipc_multiple_senders_send);

  return g_test_run ();
}
