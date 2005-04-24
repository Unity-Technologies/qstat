/*
 * qstat 2.6
 * by Steve Jankowski
 *
 * qserver functions and definitions
 * Copyright 2004 Ludwig Nussel
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 */
#ifndef QSTAT_QSERVER_H
#define QSTAT_QSERVER_H

struct query_param  {
    char *key;
    char *value;
    int i_value;
    unsigned int ui_value;
    struct query_param *next;
};

typedef struct SavedData  {
    char *data;
    int datalen;
    int pkt_index;
    int pkt_max;
    unsigned int pkt_id;
    struct SavedData *next;
} SavedData;

struct qserver {
    char *arg;
    char *host_name;
    unsigned int ipaddr;
    int flags;
    server_type * type;
    int fd;
    char *outfilename;
    char *query_arg;
    struct query_param *params;
    unsigned short port;
    /** \brief number of retries _left_ for status query or rule query.
     *
     * That means
     * if s->retry1 == (global)n_retries then no retries were necessary so far.
     * if s->retry1 == 0 then the server has to be cleaned up after timeout */
    int retry1;
    /** \brief number retries _left_ for player query. @see retry1 */
    int retry2;
    /** \brief how much retry packets were sent */
    int n_retries;
    /** \brief time when the last packet to the server was sent */
    struct timeval packet_time1;
    struct timeval packet_time2;
    /** \brief sum of packet deltas
     *
     * average server ping is ping_total / n_requests
     */
    int ping_total;

    /** \brief number of requests send to a server.
     *
     * used for ping calculation
     * @see ping_total
     */
    int n_requests;

    /** \brief number of packets already sent to this server
     *
     * doesn't seemt to have any use
     */
    int n_packets;

    /** \brief number of servers in master_pkt
     * 
     * normally master_pkt_len/6
     */
    int n_servers;
    /** \brief length of master_pkt */
    int master_pkt_len;
    /** \brief IPs received from master.
     * 
     * array of four bytes ip, two bytes port in network byte order
     */
    char *master_pkt;
    /** \brief state info
     *
     * used for progressive master 4 bytes for WON 22 for Steam
     */
    char master_query_tag[22];
    char *error;

    /** \brief in-game name of the server.
     *
     * A server that has a NULL name did not receive any packets yet and is
     * considered down after a timeout.
     */
    char *server_name;
    char *address;
    char *map_name;
    char *game;
    int max_players;
    int num_players;
    int protocol_version;

    SavedData saved_data;

    /** \brief number of the next player to retrieve info for.
     *
     * Only meaningful for servers that have type->player_packet.
     * This is used by q1 as it sends packets for each player individually.
     * cleanup_qserver() cleans up a server if next_rule == NULL and
     * next_player_info >= num_players
     */
    int next_player_info;
    /** \brief number of player info packets received */
    int n_player_info;
    struct player *players;

    /** \brief name of next rule to retreive
     *
     * Used by Q1 as it needs to send a packet for each rule. Other games would
     * set this to an empty string to indicate that rules need to be retrieved.
     * After rule packet is received set this to NULL.
     */
    char *next_rule;
    int n_rules;
    struct rule *rules;
    struct rule **last_rule;
    int missing_rules;

    struct qserver *next;
    struct qserver *prev;
};

void qserver_disconnect(struct qserver* server);

/* server specific query parameters */

void add_query_param( struct qserver *server, char *arg);

/** \brief get a parameter for the server as string
 *
 * @param server the server to get the value from
 * @param key which key to get
 * @param default_value value to return if key was not found
 * @return value for key or default_value
 */
char * get_param_value( struct qserver *server, char *key, char *default_value);

/** @see get_param_value */
int get_param_i_value( struct qserver *server, char *key, int default_value);

/** @see get_param_value */
unsigned int get_param_ui_value( struct qserver *server, char *key,
		unsigned int default_value);

/** \brief send an initial query packet to a server
 *
 * Sends a unicast packet to the server's file descriptor. The descriptor must
 * be connected. Updates n_requests or n_retries, retry1, n_packets, packet_time1
 *
 * \param server the server
 * \param data data to send
 * \param len length of data
 * \returns number of bytes sent or SOCKET_ERROR
 */
int qserver_send_initial(struct qserver* server, const char* data, size_t len);

/** \brief send an initial query packet to a server
 *
 * Sends a unicast packet to the server's file descriptor. The descriptor must
 * be connected. Updates n_requests, n_packets, packet_time1. Sets retry1 to
 * (global) n_retries
 *
 * @see qserver_send_initial
 */
int qserver_send(struct qserver* server, const char* data, size_t len);

int send_broadcast( struct qserver *server, const char *pkt, size_t pktlen);

#endif
