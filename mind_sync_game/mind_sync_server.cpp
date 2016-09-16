/*
 * mind_sync_server.cpp: This file contains all the important server functions
 * required by the main server.
*/
#include <algorithm>
#include <iostream>
#include <locale>
#include <string>
#include <vector>
#include <sstream>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
		
#define BUFSIZE	2048
#define NAMELEN	100
#define WHITE_SPACE " \t\n"

int errexit(const char *format, ...);
using namespace std;

/*
 * trim: Function to remove random characters from input username and password. 
*/
string 
trim(string str) {
	string ans;
	size_t i;
	for (i=0; i < str.size(); ++i) {
		if (isalnum(str[i])) {
			ans = str.substr(i);
			break;
		}
	}
	
	for(i=ans.size() - 1; i >= 0; --i) {
		if (isalnum(ans[i])) {
			break;
		}
	}
	
	ans = ans.substr(0, i+1);
	return ans;
}

// Function to split a string in tokens by a delimiter
vector<string> split(string str, char delimiter) {
  vector<string> internal;
  stringstream ss(str); // Turn the string into a stream.
  string tok;
  
  while(getline(ss, tok, delimiter)) {
    internal.push_back(tok);
  }
  
  return internal;
}
