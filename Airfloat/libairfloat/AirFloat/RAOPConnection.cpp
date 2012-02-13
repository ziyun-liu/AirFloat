
//
//  RAOPConnection.cpp
//  AirFloatCF
//
//  Created by Kristian Trenskow on 5/10/11.
//  Copyright 2011 The Famous Software Company. All rights reserved.
//

#include <stdio.h>
#include <sys/socket.h>
#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/aes.h>
#include <openssl/md5.h>
#include <netinet/in.h>
#include <ifaddrs.h>
#include <net/if_dl.h>

#include <AudioToolbox/AudioToolbox.h>

#include "DMAPParser.h"
#include "NotificationCenter.h"
#include "Log.h"
#include "Base64.h"
#include "RAOPHeader.h"
#include "RAOPParameters.h"
#include "RAOPConnection.h"

#define AIRPORT_PRIVATE_KEY \
"-----BEGIN RSA PRIVATE KEY-----\n" \
"MIIEpQIBAAKCAQEA59dE8qLieItsH1WgjrcFRKj6eUWqi+bGLOX1HL3U3GhC/j0Qg90u3sG/1CUt\n" \
"wC5vOYvfDmFI6oSFXi5ELabWJmT2dKHzBJKa3k9ok+8t9ucRqMd6DZHJ2YCCLlDRKSKv6kDqnw4U\n" \
"wPdpOMXziC/AMj3Z/lUVX1G7WSHCAWKf1zNS1eLvqr+boEjXuBOitnZ/bDzPHrTOZz0Dew0uowxf\n" \
"/+sG+NCK3eQJVxqcaJ/vEHKIVd2M+5qL71yJQ+87X6oV3eaYvt3zWZYD6z5vYTcrtij2VZ9Zmni/\n" \
"UAaHqn9JdsBWLUEpVviYnhimNVvYFZeCXg/IdTQ+x4IRdiXNv5hEewIDAQABAoIBAQDl8Axy9XfW\n" \
"BLmkzkEiqoSwF0PsmVrPzH9KsnwLGH+QZlvjWd8SWYGN7u1507HvhF5N3drJoVU3O14nDY4TFQAa\n" \
"LlJ9VM35AApXaLyY1ERrN7u9ALKd2LUwYhM7Km539O4yUFYikE2nIPscEsA5ltpxOgUGCY7b7ez5\n" \
"NtD6nL1ZKauw7aNXmVAvmJTcuPxWmoktF3gDJKK2wxZuNGcJE0uFQEG4Z3BrWP7yoNuSK3dii2jm\n" \
"lpPHr0O/KnPQtzI3eguhe0TwUem/eYSdyzMyVx/YpwkzwtYL3sR5k0o9rKQLtvLzfAqdBxBurciz\n" \
"aaA/L0HIgAmOit1GJA2saMxTVPNhAoGBAPfgv1oeZxgxmotiCcMXFEQEWflzhWYTsXrhUIuz5jFu\n" \
"a39GLS99ZEErhLdrwj8rDDViRVJ5skOp9zFvlYAHs0xh92ji1E7V/ysnKBfsMrPkk5KSKPrnjndM\n" \
"oPdevWnVkgJ5jxFuNgxkOLMuG9i53B4yMvDTCRiIPMQ++N2iLDaRAoGBAO9v//mU8eVkQaoANf0Z\n" \
"oMjW8CN4xwWA2cSEIHkd9AfFkftuv8oyLDCG3ZAf0vrhrrtkrfa7ef+AUb69DNggq4mHQAYBp7L+\n" \
"k5DKzJrKuO0r+R0YbY9pZD1+/g9dVt91d6LQNepUE/yY2PP5CNoFmjedpLHMOPFdVgqDzDFxU8hL\n" \
"AoGBANDrr7xAJbqBjHVwIzQ4To9pb4BNeqDndk5Qe7fT3+/H1njGaC0/rXE0Qb7q5ySgnsCb3DvA\n" \
"cJyRM9SJ7OKlGt0FMSdJD5KG0XPIpAVNwgpXXH5MDJg09KHeh0kXo+QA6viFBi21y340NonnEfdf\n" \
"54PX4ZGS/Xac1UK+pLkBB+zRAoGAf0AY3H3qKS2lMEI4bzEFoHeK3G895pDaK3TFBVmD7fV0Zhov\n" \
"17fegFPMwOII8MisYm9ZfT2Z0s5Ro3s5rkt+nvLAdfC/PYPKzTLalpGSwomSNYJcB9HNMlmhkGzc\n" \
"1JnLYT4iyUyx6pcZBmCd8bD0iwY/FzcgNDaUmbX9+XDvRA0CgYEAkE7pIPlE71qvfJQgoA9em0gI\n" \
"LAuE4Pu13aKiJnfft7hIjbK+5kyb3TysZvoyDnb3HOKvInK7vXbKuU4ISgxB2bB3HcYzQMGsz1qJ\n" \
"2gG0N5hvJpzwwhbhXqFKA4zaaSrw622wDniAK5MlIE0tIAKKP4yxNGjoD2QYjhBGuhvkWKaXTyY=\n" \
"-----END RSA PRIVATE KEY-----"

#define MIN(x, y) (x < y ? x : y)

static char* appendBuffer(char* buffer, bool nl, const char* format, ...) {
    
    char buf[1000];
    char formatnl[strlen(format) + 3];
    if (nl)
        sprintf(formatnl, "%s\r\n", format);
    else
        strcpy(formatnl, format);
    
    va_list args;
    va_start(args, format);
    vsnprintf(buf, 1000, formatnl, args);
    va_end(args);
    
    long bufLength = strlen(buf);
    
    buffer = (char*)realloc(buffer, strlen(buffer) + bufLength + 2);
    memcpy(&buffer[strlen(buffer)], buf, bufLength + 1);
    
    return buffer;
    
}

static char* interfaceForIp(struct sockaddr_in6* addr) {
    
    struct ifaddrs* if_addrs = NULL;
    struct ifaddrs* if_addr = NULL;
    char* ret = NULL;
    
    if (0 == getifaddrs(&if_addrs)) {
        
        for (if_addr = if_addrs ; if_addr != NULL ; if_addr = if_addr->ifa_next) {
            
            unsigned char ip[16];
            memset(ip, 0, 16);
            if (if_addr->ifa_addr->sa_family == AF_INET6 || if_addr->ifa_addr->sa_family == AF_INET) {
                
                if (if_addr->ifa_addr->sa_family == AF_INET6)
                    memcpy(ip, ((struct sockaddr_in6*)if_addr->ifa_addr)->sin6_addr.s6_addr, 16);
                else {
                    
                    ip[10] = ip[11] = 0xFF;
                    memcpy(&ip[12], &((struct sockaddr_in*)if_addr->ifa_addr)->sin_addr.s_addr, 4);
                    
                }
                
                if (memcmp(addr->sin6_addr.s6_addr, ip, 16) == 0) {
                    ret = (char*)malloc(strlen(if_addr->ifa_name) + 1);
                    strcpy(ret, if_addr->ifa_name);
                    break;
                }
                
            }
                        
        }
        
    }
    
    freeifaddrs(if_addrs);
    
    return ret;
    
}

static char* hwAddressForInterface(const char* interface) {
    
    struct ifaddrs* if_addrs = NULL;
    struct ifaddrs* if_addr = NULL;
    char* ret = (char*)malloc(6);
    memset(ret, 0, 6);
    
    if (0 == getifaddrs(&if_addrs))        
        for (if_addr = if_addrs ; if_addr != NULL ; if_addr = if_addr->ifa_next) {
            
            if (if_addr->ifa_name != NULL && if_addr->ifa_addr->sa_family == AF_LINK && strcmp(interface, if_addr->ifa_name) == 0) {
                
                struct sockaddr_dl* sdl = (struct sockaddr_dl*)if_addr->ifa_addr;
                if (sdl->sdl_alen == 6) {
                    memcpy(ret, LLADDR(sdl), sdl->sdl_alen);
                    break;
                }
                
            }
            
        }
    
    freeifaddrs(if_addrs);
    
    if (0 == memcmp(ret, "\0\0\0\0\0\0", 6)) {
        free(ret);
        return NULL;
    }
    
    return ret;
    
}

static struct in_addr ipv4ForInterface(const char* interface) {
    
    struct ifaddrs* if_addrs = NULL;
    struct ifaddrs* if_addr = NULL;
    in_addr ret;
    
    if (0 == getifaddrs(&if_addrs))        
        for (if_addr = if_addrs ; if_addr != NULL ; if_addr = if_addr->ifa_next) {
            
            if (if_addr->ifa_name != NULL && if_addr->ifa_addr->sa_family == AF_INET && strcmp(interface, if_addr->ifa_name) == 0) {
                
                memcpy(&ret, &((struct sockaddr_in*)if_addr->ifa_addr)->sin_addr, sizeof(struct in_addr));
                break;
                
            }
            
        }
    
    freeifaddrs(if_addrs);
    
    return ret;                
    
}

static struct in6_addr ipv6ForInterface(const char* interface) {
    
    struct ifaddrs* if_addrs = NULL;
    struct ifaddrs* if_addr = NULL;
    in6_addr ret;
    
    if (0 == getifaddrs(&if_addrs))
        for (if_addr = if_addrs ; if_addr != NULL ; if_addr = if_addr->ifa_next) {
            
            if (if_addr->ifa_next != NULL && if_addr->ifa_addr->sa_family == AF_INET6 && strcmp(interface, if_addr->ifa_name) == 0) {
                
                memcpy(&ret, &((struct sockaddr_in6*)if_addr->ifa_addr)->sin6_addr, sizeof(struct in6_addr));
                break;
                
            }
            
        }

    freeifaddrs(if_addrs);
    
    return ret;
    
}

static RSA* applePrivateKey() {
    
    BIO* bio = BIO_new(BIO_s_mem());
    BIO_write(bio, AIRPORT_PRIVATE_KEY, strlen(AIRPORT_PRIVATE_KEY));
    RSA *rsa = PEM_read_bio_RSAPrivateKey(bio, NULL, NULL, NULL); //NULL, NULL, NULL);
    BIO_free(bio);
    log(LOG_INFO, "Apple key: %s", (RSA_check_key(rsa) ? "OK" : "Failed"));
    
    return rsa;
    
}

static void binaryIP(struct in_addr localaddr, unsigned char* buf) {
    
    char ip[50];
    inet_ntop(AF_INET, &localaddr.s_addr, ip, 50);
    
    char* parts[4] = { NULL, NULL, NULL, NULL };
    parts[0] = ip;
    int p = 1;
    long l = strlen(ip);
    for (int i = 0 ; i < l ; i++)
        if (ip[i] == '.') {
            if (p < 4)
                parts[p++] = &ip[i+1];
            ip[i] = '\0';
        }
    
    for (int i = 0 ; i < 4 ; i++) {
        int p;
        sscanf(parts[i], "%d", &p);
        buf[i] = (char)p;
    }
    
}

static long padBase64(const char* base64, long length, char* out) {
    
    memcpy(out, base64, length);
    for (long i = 0 ; i < (length % 4) ; i++)
        out[length + i] = '=';
    out[length + (length % 4)] = '\0';
    
    return length + (length % 4);
    
}

static void md5ToString(unsigned char* md5, char* out) {
    
    char hexValues[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
    
    out[32] = '\0';
    for (int i = 0 ; i < 16 ; i++) {
        out[i*2] = hexValues[(md5[i] >> 4) & 0xF];
        out[(i*2)+1] = hexValues[md5[i] & 0xF];
    }
    
}

static void md5(const char* content, size_t length, unsigned char* md5) {
    
    MD5_CTX context;
    MD5_Init(&context);
    MD5_Update(&context, content, length);
    MD5_Final(md5, &context);
    
}

const char* RAOPConnection::recordingStartedNotificationName = "connectionRecordingStarted";
const char* RAOPConnection::clientDisconnectedNotificationName = "connectionClientDisconnected";
const char* RAOPConnection::clientUpdatedMetadataNotificationName = "connectionClientUpdatedMetadata";
const char* RAOPConnection::clientUpdatedProgressNotificationName = "connectionClientUpdatedProgress";
const char* RAOPConnection::clientUpdatedTrackInfoNofificationName = "connectionClientUpdatedTrackInfo";

RAOPConnection::RAOPConnection(Socket* sock) {
        
    _sock = sock;
    
    _rtp = NULL;
    
    Boolean authenticateIsSet = false;
    Boolean authenticate = CFPreferencesGetAppBooleanValue(CFSTR("authenticate"), kCFPreferencesCurrentApplication, &authenticateIsSet);
    
    _authenticationEnabled = (authenticateIsSet != 0 && authenticate != 0);    
    _password = (CFStringRef) CFPreferencesCopyAppValue(CFSTR("pw"), kCFPreferencesCurrentApplication);

    memset(_digestNonce, 0, 33);
    
    _dacpId[0] = _activeRemote[0] = _sessionId[0] = '\0';
        
}

RAOPConnection::~RAOPConnection() {
    
    delete _sock;
    
    if (_rtp != NULL) {
        delete _rtp;
        _rtp = NULL;
    }
    
    if (_password != NULL)
        CFRelease(_password);
    
    log(LOG_INFO, "Connection cleanup complete");
    
}

RTPReceiver* RAOPConnection::getRTPReceiver() {
    
    return _rtp;
    
}

SocketEndPoint RAOPConnection::getRemoteEndPoint() {
    
    SocketEndPoint ret;
    
    if (_sock->GetRemoteEndPoint() != NULL)
        ret = *_sock->GetRemoteEndPoint();
    
    return ret;
    
}

void* RAOPConnection::_connectionLoopKickStarter(void* t) {
 
    pthread_setname_np("Client Connection Socket");
    ((RAOPConnection*)t)->_connectionLoop();
    pthread_exit(0);
    
}

long RAOPConnection::_convertNewLines(unsigned char* buffer, long length) {
    
    for (long i = 0; i < length; i++) {
        if (buffer[i] == '\r')
            if (i < length - 1 && buffer[i+1] == '\n') { // if newline is \r\n then remove \n. Else make \r to \n.
                memcpy(&buffer[i], &buffer[i+1], length - (i + 1));
                i--;
                length--;
            } else
                buffer[i] = '\n';
        if (i < length - 1 && buffer[i] == '\n' && buffer[i+1] == '\n')
            break;
    }
    
    
    return length;
    
}

void RAOPConnection::_appleResponse(const char* challenge, long length, char* response, long* resLength) {
    
    unsigned char decodedChallenge[1000];
    int actualLength = base64_decode(challenge, decodedChallenge);
    
    if (actualLength != 16)
        log(LOG_ERROR, "Apple-Challenge: Expected 16 bytes - got %d", actualLength);
    
    char* interface = interfaceForIp((struct sockaddr_in6*)_sock->GetLocalEndPoint()->getSocketAddress());
    
    if (interface != NULL) {
        
        int responseSize = 32;
        char aResponse[48]; // IPv6 responds with 48 bytes
        
        char* hwid = hwAddressForInterface(interface);
        memset(aResponse, 0, sizeof(aResponse));
        
        if (hwid != NULL) {
            
            if (!IN6_IS_ADDR_V4MAPPED(&((struct sockaddr_in6*)_sock->GetRemoteEndPoint()->getSocketAddress())->sin6_addr)) {
                
                responseSize = 48;
                
                struct in6_addr in6addr = ipv6ForInterface(interface);
                
                memcpy(aResponse, decodedChallenge, actualLength);
                memcpy(&aResponse[actualLength], &in6addr, 16);
                memcpy(&aResponse[actualLength + 16], hwid, 6);
                
            } else {
                
                struct in_addr ip4addr = ipv4ForInterface(interface);
                
                unsigned char ip4[4];
                binaryIP(ip4addr, ip4);
                
                memcpy(aResponse, decodedChallenge, actualLength);
                memcpy(&aResponse[actualLength], ip4, 4);
                memcpy(&aResponse[actualLength + 4], hwid, 6);
                
            }
            
        } else
            log(LOG_ERROR, "Error getting hardware ID");
        
        RSA* rsa = applePrivateKey();
        
        int size = RSA_size(rsa);
        unsigned char clearResponse[size];
        memset(clearResponse, 0xFF, size);
        clearResponse[0] = 0;
        clearResponse[1] = 1;
        clearResponse[size - (responseSize + 1)] = 0;
        memcpy(&clearResponse[size - responseSize], aResponse, responseSize);
        
        unsigned char encryptedResponse[size];
        size = RSA_private_encrypt(size, clearResponse, encryptedResponse, rsa, RSA_NO_PADDING);
        
        if (size >= 0) {
            
            char* aEncryptedResponse;
            long aLen = base64_encode(encryptedResponse, size, &aEncryptedResponse);
            while (aLen > 1 && aEncryptedResponse[aLen - 1] == '=') {
                aEncryptedResponse[aLen - 1] = '\0';
                aLen--;
            }
            
            if (response != NULL)
                memcpy(response, aEncryptedResponse, aLen);
            if (resLength != NULL)
                *resLength = aLen;
            
            free(aEncryptedResponse);
            
        } else {
            log(LOG_ERROR, "Unable to encrypt Apple response");
            if (resLength != NULL)
                *resLength = 0;
        }
        
        RSA_free(rsa);
        free(hwid);
        free(interface);
        
    } else {
        log(LOG_ERROR, "Cound not fint interface for IP");
        if (resLength != NULL)
            *resLength = 0;
    }
        
}

bool RAOPConnection::_checkAuthentication(const char* method, const char* uri, const char* authenticationParameter) {
    
    assert(method != NULL && uri != NULL);
    
    if (_authenticationEnabled && _password && CFStringGetLength(_password) > 0) {
        
        if (CFStringGetLength(_password) > 0) {
            
            if (authenticationParameter != NULL) {
                
                const char* paramBegin = strstr(authenticationParameter, " ") + 1;
                if (paramBegin) {
                    RAOPParameters* parameters = new RAOPParameters(paramBegin, strlen(paramBegin), ParameterTypeDigest);
                    
                    const char* nonce = parameters->valueForParameter("nonce");
                    const char* response = parameters->valueForParameter("response");
                    
                    char wResponse[strlen(response) + 1];
                    strcpy(wResponse, response);
                    
                    // Check if nonce is correct
                    if (nonce != NULL && strlen(nonce) == 32 && strcmp(nonce, _digestNonce) == 0) {
                        
                        const char* username = parameters->valueForParameter("username");
                        const char* realm = parameters->valueForParameter("realm");
                        int pwLen = CFStringGetLength(_password);
                        const char* password = CFStringGetCStringPtr(_password, CFStringGetFastestEncoding(_password));
                        
                        char a1pre[strlen(username) + strlen(realm) + pwLen + 3];
                        sprintf(a1pre, "%s:%s:%s", username, realm, password);
                        
                        char a2pre[strlen(method) + strlen(uri) + 2];
                        sprintf(a2pre, "%s:%s", method, uri);
                        
                        unsigned char a1[16], a2[16];
                        md5(a1pre, strlen(a1pre), a1);
                        md5(a2pre, strlen(a2pre), a2);
                        
                        char ha1[33], ha2[33];
                        md5ToString(a1, ha1);
                        md5ToString(a2, ha2);
                        
                        char finalpre[67 + strlen(_digestNonce)];
                        sprintf(finalpre, "%s:%s:%s", ha1, _digestNonce, ha2);
                        
                        unsigned char final[16];
                        md5(finalpre, strlen(finalpre), final);
                        
                        char hfinal[33];
                        md5ToString(final, hfinal);
                        
                        for (int i = 0 ; i < 32 ; i++) {
                            hfinal[i] = tolower(hfinal[i]);
                            wResponse[i] = tolower(wResponse[i]);
                        }
                        
                        if (strcmp(hfinal, wResponse) == 0) {
                            log(LOG_INFO, "Authentication success");
                            return true;
                        }
                        
                    }
                    
                }
                
            } else
                log(LOG_INFO, "Authentication header missing");

        }
        
        log(LOG_INFO, "Authentication failed!");
        return false;
        
    }
    
    return true;
    
}

RAOPConnectionAudioRoute RAOPConnection::_getAudioRoute() {
    
    CFStringRef route = NULL;
    UInt32 size = sizeof(CFStringRef);
    AudioSessionGetProperty(kAudioSessionProperty_AudioRoute, &size, &route);
    
    int length = CFStringGetLength(route);
    if (route != NULL && length > 0) {
        
        char cRoute[length+1];
        if (CFStringGetCString(route, cRoute, length+1, kCFStringEncodingASCII)) {
            log(LOG_INFO, "Audio route: %s", cRoute);
            if (strcmp(cRoute, "Headphone") == 0)
                return kRAOPConnectionAudio_RouteHeadphones;
            else if (strcmp(cRoute, "AirTunes") == 0)
                return kRAOPConnectionAudio_RouteAirPlay;            
        }
        
    }
    
    return kRAOPConnectionAudio_RouteUnknown;
    
}

bool RAOPConnection::_processData(const char* cmd, const char* path, RAOPHeader* header, unsigned char* content, long contentLength) {
    
    int cSeq = 0;
    int responseCode = 200;
    bool success = true;
    bool end = false;
    
    char* response = (char*)malloc(1);
    char* finalResponse = (char*)malloc(1);
    finalResponse[0] = response[0] = '\0';
    
    if (cmd != NULL && path != NULL) {
    
        log(LOG_INFO, "CMD: %s / PATH: %s", cmd, path);
        
        RAOPParameters* parameters = NULL;
        
        if (header->valueForName("CSeq") != NULL)
            cSeq = atoi(header->valueForName("CSeq"));
        
        if (contentLength > 0) {
            if (strcmp(header->valueForName("Content-Type"), "application/sdp") == 0 || strcmp(header->valueForName("Content-Type"), "text/parameters") == 0) {
                int cl = _convertNewLines((unsigned char*)content, contentLength);
                if (strcmp(header->valueForName("Content-Type"), "application/sdp") == 0)
                    parameters = new RAOPParameters((char*)content, cl, ParameterTypeSDP);
                else if (strcmp(header->valueForName("Content-Type"), "text/parameters") == 0)
                    parameters = new RAOPParameters((char*)content, cl, ParameterTypeTextParameters);
            }
        }
        
        const char* dacpId;
        if (_dacpId[0] == '\0' && (dacpId = header->valueForName("DACP-ID")) != NULL)
            strncpy(_dacpId, dacpId, DACPID_MAXLENGTH);
        
        const char* activeRemote;
        if (_activeRemote[0] == '\0' && (activeRemote = header->valueForName("Active-Remote")) != NULL)
            strncpy(_activeRemote, activeRemote, ACTIVEREMOTE_MAXLENGTH);
        
        const char *sessionId;
        if (_sessionId[0] == '\0' && (sessionId = header->valueForName("Session")) != NULL)
            strncpy(_sessionId, sessionId, SESSION_MAXLENGTH);
        
        response = appendBuffer(response, true, "Server: AirTunes/104.29");
        
        response = appendBuffer(response, true, "CSeq: %d", cSeq);
        
        if (_checkAuthentication(cmd, path, header->valueForName("Authorization"))) {
            
            if (0 == strcmp(cmd, "OPTIONS"))
                response = appendBuffer(response, true, "Public: ANNOUNCE, RECORD, PAUSE, FLUSH, TEARDOWN, OPTIONS, GET_PARAMETER, SET_PARAMETER, POST, GET");
            else if (0 == strcmp(cmd, "ANNOUNCE")) {
                
                const char* codec = NULL;
                int codecIdentifier;
                
                const char* rtpmap;
                if ((rtpmap = parameters->valueForParameter("a-rtpmap")) != NULL) {
                    
                    codecIdentifier = atoi(rtpmap);
                    for (long i = 0 ; i < strlen(rtpmap) - 1 ; i++)
                        if (rtpmap[i] == ' ') {
                            codec = &rtpmap[i+1];
                            break;
                        }
                    
                    const char* fmtp;
                    if ((fmtp = parameters->valueForParameter("a-fmtp")) != NULL && atoi(fmtp) == codecIdentifier) {
                        
                        int size = 0;
                        size_t fmtpLen = strlen(fmtp);
                        for (long i = 0 ; i < fmtpLen ; i++)
                            if (fmtp[i] == ' ')
                                size++;
                        
                        int fmtps[size];
                        int cfmtp = 0;
                        for (long i = 0 ; i < fmtpLen ; i++)
                            if (fmtp[i] == ' ')
                                fmtps[cfmtp++] = atoi(&fmtp[i +1]);
                        
                        char localHost[100];
                        char remoteHost[100];
                        _sock->GetLocalEndPoint()->getHost(localHost, 100);
                        _sock->GetRemoteEndPoint()->getHost(remoteHost, 100);
                        
                        unsigned char aeskey[16];
                        unsigned char aesiv[16];
                        
                        bzero(aeskey, 16);
                        bzero(aesiv, 16);
                        
                        if (_rtp != NULL) {
                            delete _rtp;
                            _rtp = NULL;
                        }
                        
                        const char* aesKeyBase64Encoded;
                        if ((aesKeyBase64Encoded = parameters->valueForParameter("a-rsaaeskey")) != NULL) {
                            
                            char aesKeyBase64EncodedPadded[strlen(aesKeyBase64Encoded) + 5];
                            long size = padBase64(aesKeyBase64Encoded, strlen(aesKeyBase64Encoded), aesKeyBase64EncodedPadded);
                            char aesKeyEncryptet[size];
                            size = base64_decode(aesKeyBase64EncodedPadded, aesKeyEncryptet);
                            
                            RSA* rsa = applePrivateKey();
                            unsigned char aesKey[size + 1];
                            size = RSA_private_decrypt((int)size, (unsigned char*)aesKeyEncryptet, aesKey, rsa, RSA_PKCS1_OAEP_PADDING);
                            
                            if (size >= 16)
                                memcpy(aeskey, aesKey, 16);
                            
                            log(LOG_INFO, "AES key length: %d bits", size * 8);
                            
                            const char* aesInitializerBase64Encoded = parameters->valueForParameter("a-aesiv");
                            char aesInitializerBase64EncodedPadded[strlen(aesInitializerBase64Encoded) + 5];
                            size = padBase64(aesInitializerBase64Encoded, strlen(aesInitializerBase64Encoded), aesInitializerBase64EncodedPadded);
                            char aesInitializer[size];
                            size = base64_decode(aesInitializerBase64EncodedPadded, aesInitializer);
                            
                            if (size >= 16)
                                memcpy(aesiv, aesInitializer, 16);
                            
                            log(LOG_INFO, "AES initialization vector length: %d bits", size * 8);
                            
                            _rtp = new RTPReceiver(localHost, remoteHost, aeskey, aesiv, this, fmtps, size);
                            
                        } else // Stream is clear text
                            _rtp = new RTPReceiver(localHost, remoteHost, this, fmtps, size);
                        
                    } else
                        responseCode = 400; // Bad request
                    
                } else                        
                    responseCode = 400; // Bad request
                
            } else if (0 == strcmp(cmd, "SETUP")) {
                
                if (RTPReceiver::isAvailable() && _getAudioRoute() != kRAOPConnectionAudio_RouteAirPlay) {
                    
                    const char* transport;
                    if (_rtp != NULL && (transport = header->valueForName("Transport"))) {
                        
                        char session[10];
                        _rtp->getSession(session);
                        response = appendBuffer(response, true, "Session: %02X%02X%02X%02X", session[0], session[1], session[2], session[3]);
                        
                        RAOPParameters* transportParams = new RAOPParameters(transport, strlen(transport), ParameterTypeSDP, ';');
                        
                        unsigned short timing_port, control_port;
                        timing_port = control_port = 0;
                        
                        const char *tp, *cp;
                        if ((tp = transportParams->valueForParameter("timing_port"))) {
                            timing_port = atoi(tp);
                            transportParams->removeParameter("timing_port");
                        }
                        if ((cp = transportParams->valueForParameter("control_port"))) {
                            control_port = atoi(cp);
                            transportParams->removeParameter("control_port");
                        }
                        
                        unsigned short port;
                        if ((port = _rtp->setup(&timing_port, &control_port))) {
                            
                            response = appendBuffer(response, false, "Transport: ");
                            
                            for (long i = 0 ; i < transportParams->count() ; i++) {
                                const char* key = transportParams->parameterAtIndex(i);
                                const char* value;
                                if ((value = transportParams->valueForParameter(key)) != key)
                                    response = appendBuffer(response, false, "%s=%s;", key, value);
                                else
                                    response = appendBuffer(response, false, "%s;", key);
                            }
                            
                            if (control_port > 0)
                                response = appendBuffer(response, false, "control_port=%d;", control_port);
                            if (timing_port > 0)
                                response = appendBuffer(response, false, "timing_port=%d;", timing_port);
                            
                            response = appendBuffer(response, true, "server_port=%d", port);
                            
                            log(LOG_INFO, "RTP receiver set up at port %d", port);
                            
                        } else
                            log(LOG_ERROR, "RTP Receiver didn't start");
                        
                    } else                            
                        responseCode = 400; // Bad request
                    
                } else 
                    responseCode = 453; // Not enough bandwidth;
                
            } else if (0 == strcmp(cmd, "RECORD")) {
                
                char session[10];
                _rtp->getSession(session);
                response = appendBuffer(response, true, "Session: %02X%02X%02X%02X", session[0], session[1], session[2], session[3]);
                
                const char* rtpinfo;
                if ((rtpinfo = header->valueForName("RTP-Info"))) {
                    
                    RAOPParameters* rtpParams = new RAOPParameters(rtpinfo, strlen(rtpinfo), ParameterTypeSDP, ';');
                    _rtp->start(atoi(rtpParams->valueForParameter("seq")));
                    
                    delete rtpParams;
                    
                } else {
                    _rtp->start();
                }
                
                NotificationCenter::defaultCenter()->postNotification(RAOPConnection::recordingStartedNotificationName, this, NULL);
                
                response = appendBuffer(response, true, "Audio-Latency: 15049");
                
                responseCode = 200;
                
            } else if (0 == strcmp(cmd, "SET_PARAMETER")) {
                
                if (parameters != NULL) {
                    
                    const char* volume;
                    const char* progress;
                    if ((volume = parameters->valueForParameter("volume"))) {
                        float fVolume;
                        sscanf(volume, "%f", &fVolume);
                        log(LOG_INFO, "Client set volume: %f", (30.0 + fVolume) / 30.0);
                        
                        fVolume = (fVolume == -144 ? 0.0 : (30.0 + fVolume) / 30.0);
                        _rtp->getAudioPlayer()->setVolume(fVolume * fVolume * fVolume);
                        
                    } else if ((progress = parameters->valueForParameter("progress"))) {
                        
                        const char* parts[3] = { NULL, NULL, NULL };
                        int cPart = 1;
                        parts[0] = progress;
                        
                        for (int i = 0 ; i < strlen(progress) ; i++)
                            if (progress[i] == '/')
                                parts[cPart++] = &progress[i+1];
                        
                        double start = atoi(parts[0]);
                        
                        RAOPConnectionClientUpdatedProgressNotificationInfo info;
                        info.position = (atoi(parts[1]) - start) / _rtp->getAudioPlayer()->getSampleRate();
                        info.total = (atoi(parts[2]) - start) / _rtp->getAudioPlayer()->getSampleRate();
                        
                        NotificationCenter::defaultCenter()->postNotification(RAOPConnection::clientUpdatedProgressNotificationName, this, &info);
                        
                    }
                    
                } else {
                    
                    const char* contentType = header->valueForName("Content-Type");
                    
                    if (0 == strcmp(contentType, "application/x-dmap-tagged")) {
                        
                        DMAPParser* tags = new DMAPParser(content, contentLength);
                        
                        uint32_t containerTag;
                        if (tags->count() > 0 && DMAPParser::typeForTag((containerTag = tags->tagAtIndex(0))) == kDMAPTypeContainer) {
                            
                            DMAPParser* trackTags = tags->containerForAtom(containerTag);
                            NotificationCenter::defaultCenter()->postNotification(RAOPConnection::clientUpdatedTrackInfoNofificationName, this, trackTags);
                            
                            const char* title = trackTags->stringForIdentifier("dmap.itemname");
                            const char* artist = trackTags->stringForIdentifier("daap.songartist");
                            if (title != NULL && artist != NULL)
                                log(LOG_INFO, "Now playing: \"%s\" by %s", title, artist);
                            
                        }
                        
                        delete tags;
                        
                    } else {
                    
                        log(LOG_INFO, "Track info received (Content-Type: %s)", header->valueForName("Content-Type"));
                        RAOPConnectionClientUpdatedMetadataNotificationInfo info;
                        info.content = content;
                        info.contentLength = contentLength;
                        info.contentType = header->valueForName("Content-Type");
                        
                        NotificationCenter::defaultCenter()->postNotification(RAOPConnection::clientUpdatedMetadataNotificationName, this, &info);

                    }
                    
                }
                
            } else if (0 == strcmp(cmd, "FLUSH")) {
                
                int32_t lastSeq = -1;
                const char* rtpInfo;
                if ((rtpInfo = header->valueForName("RTP-Info")) != NULL) {
                    
                    RAOPParameters* rtpParams = new RAOPParameters(rtpInfo, strlen(rtpInfo), ParameterTypeSDP, ';');
                    
                    const char* seq;
                    if ((seq = rtpParams->valueForParameter("seq")) != NULL)
                        lastSeq = atoi(seq);
                }
                
                _rtp->getAudioPlayer()->flush(lastSeq);
                responseCode = 200;
                
            } else if (0 == strcmp(cmd, "TEARDOWN")) {
                
                _rtp->stop();
                response = appendBuffer(response, true, "Connection: Close");
                end = true;
                
            } else
                responseCode = 400; // Bad request
            
        } else {
            
            unsigned char nonce[16];
            
            for (int i = 0 ; i < 16 ; i++)
                nonce[i] = (char) rand() % 256;
            
            md5ToString(nonce, _digestNonce);
            
            response = appendBuffer(response, true, "WWW-Authenticate: Digest realm=\"raop\", nonce=\"%s\"", _digestNonce);
            
            responseCode = 401; // Unauthorized
            
        }
        
        const char* challenge;
        if ((challenge = header->valueForName("Apple-Challenge"))) {
            
            char ares[1000];
            long resLength = 100;
            
            char rchallenge[strlen(challenge) + 5];
            padBase64(challenge, strlen(challenge), rchallenge);
            _appleResponse(rchallenge, strlen(rchallenge), ares, &resLength);
            
            if (resLength > 0) {
                ares[resLength] = '\0';
                response = appendBuffer(response, true, "Apple-Response: %s", ares);
            }
            
        }
        
        //response = appendBuffer(response, true, "Audio-Jack-Status: %s", (_getAudioRoute() == kRAOPConnectionAudio_RouteHeadphones ? "connected; type=analog" : "disconnected"));
        response = appendBuffer(response, true, "Audio-Jack-Status: connected; type=digital");
                    
        if (parameters != NULL)
            delete parameters;
        
    } else
        responseCode = 400; // Bad request
    
    switch (responseCode) {
        case 200:
            finalResponse = appendBuffer(finalResponse, true, "RTSP/1.0 200 OK");
            finalResponse = appendBuffer(finalResponse, true, "%s", response);
            break;
            
        case 401:
            finalResponse = appendBuffer(finalResponse, true, "RTSP/1.0 401 Unauthorized");
            finalResponse = appendBuffer(finalResponse, true, "%s", response);
            break;
            
        case 453:
            finalResponse = appendBuffer(finalResponse, true, "RTSP/1.0 453 Not Enough Bandwidth");
            finalResponse = appendBuffer(finalResponse, true, "%s", response);
            break;
            
        default:
            finalResponse = appendBuffer(finalResponse, true, "RTSP/1.0 400 Bad Request\r\n");
            break;
        
    }
    
    if (finalResponse[0] != '\0') {
        log(LOG_INFO, finalResponse);
        _sock->Send(finalResponse, strlen(finalResponse));
    } else
        log(LOG_ERROR, "Cannot send: Response unavailable");
    
    if (end)
        _sock->Close();
    
    free(finalResponse);
    free(response);
    
    log(LOG_INFO, "");
    
    return success;
    
}

void RAOPConnection::_connectionLoop() {
    
    uint32_t buffersize = 0;
    unsigned char* buffer = NULL;
    uint32_t readpos = 0;
    
    for (;;) {
        
        if (READ_SIZE + readpos > buffersize) {
            buffersize += READ_SIZE;
            buffer = (unsigned char*)realloc(buffer, buffersize);
        }
        
        int32_t read = _sock->Receive(&buffer[readpos], READ_SIZE);
                
        if (read <= 0)
            break;
        
        readpos += read;
        
        unsigned char* contentStart;
        unsigned char* headerStart = buffer;
        if ((contentStart = (unsigned char*)memmem(buffer, readpos, "\r\n\r\n", 4)) != NULL) { // Find request end
            contentStart += 4;
            
            uint32_t headerLength = _convertNewLines(buffer, contentStart - headerStart);

            log_data(LOG_INFO, (char*)buffer, headerLength - 1);
            
            char* cmd = (char*) headerStart;
            char* path = NULL;
            while (headerStart[0] != '\n') {
                if (headerStart[0] == ' ') {
                    if (path == NULL)
                        path = (char*)&headerStart[1];
                    headerStart[0] = '\0';
                }
                headerStart++;
                headerLength--;
            }
            
            headerStart++;
            headerLength--;
            
            RAOPHeader headers((char*)headerStart, contentStart - headerStart);
            const char* contentLengthStr;
            unsigned char* content = NULL;
            uint32_t contentLength = 0;
            if ((contentLengthStr = headers.valueForName("Content-Length")) != NULL && (contentLength = atoi(contentLengthStr)) > 0) {
                
                content = (unsigned char*) malloc(contentLength);
                uint32_t contentReadPos = readpos - (contentStart - buffer);
                memcpy(content, contentStart, contentReadPos);
                
                while (contentReadPos < contentLength) {
                    
                    int32_t contentRead = _sock->Receive(&content[contentReadPos], contentLength - contentReadPos);
                    
                    if (contentRead < 0) {
                        log(LOG_ERROR, "Connection read error");
                        break;
                    }
                    
                    contentReadPos += contentRead;
                    
                }
                
                log(LOG_INFO, "(Content of length: %d bytes)\n", contentLength);
                
            }
            
            _processData(cmd, path, &headers, content, contentLength);
            
            if (content != NULL)
                free(content);
            
        }
        
        free(buffer);
        buffer = NULL;
        buffersize = 0;
        readpos = 0;
        
    }
    
    log(LOG_INFO, "Client disconnected");
    
    _sock->Close();
    
    NotificationCenter::defaultCenter()->postNotification(RAOPConnection::clientDisconnectedNotificationName, this, NULL);

    if (buffer != NULL)
        free(buffer);
    
    // Selfdestroyed
    delete this;

}

void RAOPConnection::_takeOver() {
    
    pthread_create(&_connectionLoopThread, NULL, _connectionLoopKickStarter, this);
    
    char ip[50];
    _sock->GetRemoteEndPoint()->getHost(ip, 50);
    
    log(LOG_INFO, "RAOPConnection (%p) took over connection from %s:%d", this, ip, _sock->GetRemoteEndPoint()->port());
    
}

const char* RAOPConnection::getDacpId() {
    
    return _dacpId;
    
}

const char* RAOPConnection::getActiveRemote() {
    
    return _activeRemote;
    
}

const char* RAOPConnection::getSessionId() {
    
    return _sessionId;
    
}

unsigned int RAOPConnection::getNetworkScopeId() {
    
    return _sock->GetLocalEndPoint()->getScopeId();
    
}
