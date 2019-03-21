#include <slsdk.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <syslog.h>
#include <iostream>
#include <string>
#include <unistd.h>

#define LOG(X,...) syslog(LOG_CRIT,X,##__VA_ARGS__)

extern "C" int p2p_create(char *id, char *key, char *address, char *p2pname, char *tmp);
extern "C" int p2p_destroy(char *p2pname, char *tmp);
extern "C" void p2p_init(void);
extern "C" void p2p_exit(void);
std::string* g_address = NULL;

class session_info{
	public:
		session_info(){
			next = NULL;
			g_session = SLSESSION_INVAILD;
		}
		session_info *next;
		SLCLIENT g_client;
		SLSESSION g_session;
		char name[128];
		char address[256];
};

session_info* ses_head = NULL;

bool session_create( ESLSessionType plugin_type, SLSESSION* session, std::string& name, SLSESSION_CALLBACK callback, session_info *ses);

void session_destroy( session_info *ses);

void client_callback( SLCLIENT client, SLCLIENT_EVENT event, unsigned long custom );
void client_callback( SLCLIENT client, SLCLIENT_EVENT event, unsigned long custom ) {
  switch( event ) {
  case SLCLIENT_EVENT_ONCONNECT:
    LOG( "connect oray server\n" );
    break;

  case SLCLIENT_EVENT_ONDISCONNECT:
    LOG( "disconnect oray server\n" );
    break;

  case SLCLIENT_EVENT_ONLOGIN:
    {
      const char* s = SLGetClientAddress( client );
      if( s ) {
        *g_address = std::string( s, strlen( s ) );
      }

      LOG( "login oray server ok(%s)\n", g_address->c_str() );
    }
    break;

  case SLCLIENT_EVENT_ONLOGINFAIL:
    LOG( "login oray server fail\n" );
    break;

  default:
    break;
  }
}


int p2p_create(char *id, char *key, char *address, char *p2pname, char *tmp){
	session_info *ses = new session_info();
  ses->g_client = SLCreateClient();
  std::string addr;
  g_address = &addr;
  int i = 0, ret = 0;
  if( ses->g_client == SLCLIENT_INVAILD ) {
    LOG( "create client fail.\n" );
	sprintf(tmp, "Oray create client fail!\n");
    return -1;
  }

  if( !SLSetClientCallback( ses->g_client, client_callback, ( unsigned long )0 ) ) {
    LOG( "set event fail.\n" );
	sprintf(tmp, "Oray set event fail!\n");
    return -1;
  }

//  if( !SLLoginWithOpenID( ses->g_client, id, key) ) {
  if( !SLClientLoginWithLicense( ses->g_client, id, key) ) {
    LOG("login server fail.\n" );
	sprintf(tmp, "Oray login server fail!\n");
    return -1;
  }

  LOG( "Init oray client ok!\n" );

  while(g_address->length() == 0){
	LOG("g_address is NULL\n");
	if(i > 9){
		LOG("server no relay!\n");
		sprintf(tmp,"Oray server no relay!\n");
		return -2;
	}
	i++;
	usleep(50*1000);
  }
  std::string name;
  if( session_create( eSLSessionType_Port, &(ses->g_session), name, NULL , ses) ) {
	LOG("create session:\n -a \"%s\" -s \"%s\"\n", g_address->c_str(), name.c_str() );
	strcpy(p2pname, name.c_str());
	strcpy(ses->name, name.c_str());
	strcpy(address, g_address->c_str());
	strcpy(ses->address, g_address->c_str());
	LOG("address is %s\n",address);
	if (ses_head == NULL){
		ses_head = ses;
	}else{
		ses->next = ses_head;
		ses_head = ses;
	}
  }else{
	LOG("create session fail!\n");
	sprintf(tmp, "Oray create session fail!\n");
	return -1;
  }

  return 0;
}

int p2p_destroy(char *name, char *tmp){
	session_info *ses_tmp;
	session_info **ses_last;
	ses_tmp = ses_head;
	ses_last = &ses_head;

	while(ses_tmp != NULL){
		if(strcmp(name, ses_tmp->name) == 0){
			session_destroy(ses_tmp);
			*ses_last = ses_tmp->next;
			delete ses_tmp;
			return 0;
		}
		ses_last = &(ses_tmp->next);
		ses_tmp = ses_tmp->next;
	}

	sprintf(tmp, "Oray not found session!\n");
	return -1;
}

void p2p_init(void){
  SLInitialize();
}

void p2p_exit(void){
  SLUninitialize();
}

bool session_create( ESLSessionType plugin_type, SLSESSION* session, std::string& name, SLSESSION_CALLBACK callback, session_info *ses) {
  SLSESSION s = SLCreateClientSession( ses->g_client, plugin_type );
  if( s == SLSESSION_INVAILD ) {
    LOG( "create session fail\n" );
    return false;
  }

  if( callback ) {
    SLSESSION_CALLBACK_PROP prop;
    prop.pfnCallback = callback;
    prop.nCustom = 0;
    SLSetClientSessionOpt( ses->g_client, s, eSLSessionOpt_callback, ( const char* )&prop, sizeof( prop ) );
  }

  const char* sname = SLGetClientSessionName( ses->g_client, s );
  if( sname ) {
    name = std::string( sname, strlen( sname ) );
  }
  *session = s;
  return true;
}

void session_destroy(session_info *ses) {
  if( ses->g_session == SLSESSION_INVAILD ) {
    return;
  }

  if( !SLDestroyClientSession( ses->g_client, ses->g_session ) ) {
    LOG( "destroy session fail\n" );
  } else {
    LOG( "destroy session ok\n" );
  }
}
