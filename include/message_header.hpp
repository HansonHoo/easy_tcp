#ifndef _MESSAGE_HEADER_HPP_
#define _MESSAGE_HEADER_HPP_

enum CMD {
    CMD_LOGIN,
    CMD_LOGIN_RESULT,
    CMD_LOGOUT,
    CMD_LOGOUT_RESULT,
    CMD_NEW_USER_JOIN,
    CMD_ERROR
};

struct DataHeader {
    DataHeader() {
        data_length = sizeof(DataHeader);
        cmd = CMD_ERROR;
    }

    short data_length;
    short cmd;
};

//DataPackage
struct Login : public DataHeader 
{
    Login() {
        data_length = sizeof(Login);
        cmd = CMD_LOGIN;
    }

    char user_name[32];
    char password[32];
    char data[32];
};

struct LoginResult : public DataHeader 
{
    LoginResult() {
        data_length = sizeof(LoginResult);
        cmd = CMD_LOGIN_RESULT;
        result = 0;
    }

    int result;
    char data[92];
};

struct Logout : public DataHeader 
{
    Logout() {
        data_length = sizeof(Logout);
        cmd = CMD_LOGOUT;
    }

    char user_name[32];
};

struct LogoutResult : public DataHeader 
{
    LogoutResult() {
        data_length = sizeof(LogoutResult);
        cmd = CMD_LOGOUT_RESULT;
    }

    int result;
};

struct NewUserJoin : public DataHeader 
{
    NewUserJoin() {
        data_length = sizeof(NewUserJoin);
        cmd = CMD_NEW_USER_JOIN;
    }
    
    int sock;
};


#endif 