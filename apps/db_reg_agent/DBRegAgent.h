/*
 * Copyright (C) 2011 Stefan Sayer
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * For a license to use the sems software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * SEMS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _DB_REGAgent_h_
#define _DB_REGAgent_h_
#include <sys/time.h>

#include <mysql++/mysql++.h>


#include <map>
using std::map;
#include <queue>
using std::queue;

#include "AmApi.h"
#include "AmSipRegistration.h"

#include "RegistrationTimer.h"

#define REG_STATUS_INACTIVE      0
#define REG_STATUS_PENDING       1
#define REG_STATUS_ACTIVE        2
#define REG_STATUS_FAILED        3
#define REG_STATUS_REMOVED       4

#define REG_STATUS_INACTIVE_S      "0"
#define REG_STATUS_PENDING_S       "1"
#define REG_STATUS_ACTIVE_S        "2"
#define REG_STATUS_FAILED_S        "3"
#define REG_STATUS_REMOVED_S       "4"

#define COLNAME_SUBSCRIBER_ID    "subscriber_id"
#define COLNAME_USER             "user"
#define COLNAME_PASS             "pass"
#define COLNAME_REALM            "realm"

#define COLNAME_STATUS           "registration_status"
#define COLNAME_EXPIRY           "expiry"
#define COLNAME_REGISTRATION_TS  "last_registration"
#define COLNAME_LAST_CODE        "last_code"
#define COLNAME_LAST_REASON      "last_reason"

#define RegistrationActionEventID 117

struct RegistrationActionEvent : public AmEvent {

  enum RegAction { Register, Deregister };

RegistrationActionEvent(RegAction action, int subscriber_id)
  : AmEvent(RegistrationActionEventID),
    action(action), subscriber_id(subscriber_id) { }

  RegAction action;
  int subscriber_id;
};

class DBRegAgent;

// separate thread for REGISTER sending, which can block for rate limiting
class DBRegAgentProcessorThread
: public AmThread,
  public AmEventQueue,
  public AmEventHandler
{

  DBRegAgent* reg_agent;
  bool stopped;

  void rateLimitWait();

  double allowance;
  struct timeval last_check;

 protected:
  void process(AmEvent* ev);

 public:
  DBRegAgentProcessorThread();
  ~DBRegAgentProcessorThread();

  void run();
  void on_stop();

};

class DBRegAgent
: public AmDynInvokeFactory,
  public AmDynInvoke,
  public AmThread,
  public AmEventQueue,
  public AmEventHandler
{

  static string joined_query;
  static string registrations_table;

  static double reregister_interval;
  static double minimum_reregister_interval;

  static bool enable_ratelimiting;
  static unsigned int ratelimit_rate;
  static unsigned int ratelimit_per;

  map<long, AmSIPRegistration*> registrations;
  map<string, long>             registration_ltags;
  map<long, RegTimer*>          registration_timers;
  AmMutex registrations_mut;

  // connection used in main DBRegAgent thread
  static mysqlpp::Connection DBConnection;

  // connection used in other threads
  static mysqlpp::Connection StatusDBConnection;

  int onLoad();

  RegistrationTimer registration_timer;
  DBRegAgentProcessorThread registration_processor;

  bool loadRegistrations();

  void createDBRegistration(long subscriber_id, mysqlpp::Connection& conn);
  void updateDBRegistration(long subscriber_id, int last_code,
			    const string& last_reason,
			    bool update_status = false, int status = 0,
			    bool update_ts=false, unsigned int expiry = 0);

  /** create registration in our list */
  void createRegistration(long subscriber_id,
			  const string& user,
			  const string& pass,
			  const string& realm);
  /** update registration in our list */
  void updateRegistration(long subscriber_id,
			  const string& user,
			  const string& pass,
			  const string& realm);

  /** remove registration */
  void removeRegistration(long subscriber_id);

  /** schedule this subscriber to REGISTER imminently */
  void scheduleRegistration(long subscriber_id);

  /** schedule this subscriber to de-REGISTER imminently*/
  void scheduleDeregistration(long subscriber_id);

  /** create a timer for that registration 
      @param subscriber_id - ID of subscription
      @param expiry        - SIP registration expiry time
      @param reg_start_ts  - start TS of the SIP registration
   */
  void setRegistrationTimer(long subscriber_id, time_t expiry, time_t reg_start_ts);

  /** clear re-registration timer and remove timer object */
  void clearRegistrationTimer(long subscriber_id);

  /** remove timer object */
  void removeRegistrationTimer(long subscriber_id);
  
  //  void run_tests();

  // amThread
  void run();
  void on_stop();

  // AmEventHandler
  void process(AmEvent* ev);

  void onSipReplyEvent(AmSipReplyEvent* ev);

  void onRegistrationActionEvent(RegistrationActionEvent* reg_action_ev);


  unsigned int expires;

  bool running;

  AmDynInvoke* uac_auth_i;

  void DIcreateRegistration(int subscriber_id, const string& user, 
			    const string& pass, const string& realm,
			    AmArg& ret);
  void DIupdateRegistration(int subscriber_id, const string& user, 
			    const string& pass, const string& realm,
			    AmArg& ret);
  void DIremoveRegistration(int subscriber_id, AmArg& ret);


 public:
  DBRegAgent(const string& _app_name);
  ~DBRegAgent();

  DECLARE_MODULE_INSTANCE(DBRegAgent);

  // DI
  // DI factory
  AmDynInvoke* getInstance() { return instance(); }
  // DI API
  void invoke(const string& method, 
	      const AmArg& args, AmArg& ret);
  /** re-registration timer callback */
  void timer_cb(RegTimer* timer, long subscriber_id, void* data2);

  friend class DBRegAgentProcessorThread;
};

#endif