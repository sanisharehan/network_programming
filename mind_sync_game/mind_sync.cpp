/*
 * mind_sync.cpp: Multi-threaded TCP server to handle game requests from clients
 * and starting a new thread for each game.
*/

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <set>
#include <utility>
#include <vector>

#include <sqlite3.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/signal.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <pthread.h>

#define BUFSIZE	2048
#define	QLEN 		32
#define	LENGTH		2000
#define NUM_THREADS 2000

// This acts as protocol between client and server.
#define USERNAMEERROR "USER_NAME_ERROR"
#define PASSWORDERROR	"PASSWORD_ERROR"
#define USERNAMEVALID	"USER_NAME_VALID"
#define PASSWORDVALID	"PASSWORD_VALID"
#define DUPLICATEUSER	"USER_ALREADY_LOGGED"
#define USERNAMETAKEN	"USER_NAME_TAKEN"
#define INVALIDRESPONSE	"INVALID_USER_INPUT"
#define GAME_END "GAME_END"
#define SUCCESS	"SUCCESS"
#define NO_RESPONSE "no_response"

#define WRITE_OUT_BUFFER if (write(sd, outBuf, strlen(outBuf)) < 0) { errexit("Error in writing to the socket\n"); }

#define WORDS_FILE_LOC "words.txt"
#define DBNAME "MindSync.db"
#define TABLENAME "Users"

#define HIGH_PRIORITY_PORT string("9000")
#define LOW_PRIORITY_PORT string("9001")

#define DB_REFRESH_RATE 30
#define SERVER_COMM_RATE 15

#define TO_LOWER(s) std::transform(s.begin(), s.end(), s.begin(), ::tolower)

using namespace std;

int connectTCP(const char* host, const char* service);
int errexit(const char *format, ...);
int passiveTCP(const char *service, int qlen);
int validateClientUsername(int sd, string& username);
string trim(string);
vector<string> split(string str, char delimiter);


// Class for lock protected operations on socket descriptors.
// This also serves as the current state of the server.
class ProtectedSockets {
	private:
    std::mutex mutex;
		// Pair is (username, socket_id/file_descriptor).
		std::vector<std::pair<string, int>> fds;
		std::set<std::pair<string, int>> free_players;
	public:
		int find_active_user_fd(const string& username) {
			int fd = -1;
			for(size_t i=0; i < fds.size(); ++i) {
				if (fds[i].first.compare(username) == 0) {
				  fd = fds[i].second;
					break;
				}
			}
			return fd;
		}

		bool if_user_alive(const string& username) {
			int fd = find_active_user_fd(username);
			if (fd == -1) {
				return false;
			} else {
				pair<string, int> player(username, fd);
				if (free_players.find(player) == free_players.end()) {
					return true;	
				} else {
			    	string send_echo_back = "SEND_ECHO_BACK";
    				write(fd, send_echo_back.c_str(), send_echo_back.size());

			    	char inBuf[BUFSIZE + 1];
					int recvLen;
    				if ((recvLen = read(fd, inBuf, BUFSIZE)) > 0) { 
						inBuf[recvLen] = '\0';        
    				}
    				string input = trim(string(inBuf));
					if (input.compare("ECHO") == 0) {
						return true;
					} else {
						search_and_delete(player);
						return false;
					}
				}
			}
		}		

		void push_back(string& username, int fd) {
			mutex.lock();
			pair<string, int> username_fd(username, fd);
			fds.push_back(username_fd);
			free_players.insert(username_fd);
			mutex.unlock();
		}	

		std::pair<string, int> operator [](int index) {
	    	return fds[index];	
		}

		void search_and_delete(std::pair<string, int>& player) {
			mutex.lock();
			auto iter = find(fds.begin(), fds.end(), player);
			fds.erase(iter);
			free_players.erase(player);
			mutex.unlock();
		}
	
		int size() {
			return fds.size();
		}

		int free_players_count() {
			return free_players.size();
		}

		pair<string, int> get_free_player() {
			mutex.lock();
			auto player = *(free_players.begin());
			free_players.erase(free_players.begin());
			mutex.unlock();
			return player;
		}
		
		void add_free_player(std::pair<string, int> player) {
			mutex.lock();
			free_players.insert(player);
			mutex.unlock();
		}
		
		void dump_state() {
			cout << "All players: " << endl;
			for (auto player : fds) {
				cout << player.first << ":" << player.second << endl;	
			}

			cout << "Free players: " << endl;
			for (auto player : free_players) {
				cout << player.first << ":" << player.second << endl;
			}
		}
};

// All the active connections.
ProtectedSockets active_connections;

// All the words loaded.
vector<string> words;

// Players who would be playing against each other. This
// struct stores username and socket descriptor.
struct GamePlayers {
	pair<string, int> player1;
	pair<string, int> player2;
};

// Class for User stats.
class User {
public:
	int id;
	string username;
	string password;
	int total_score = 0;
	int best_score = 0;
	int worst_score = 0;
	int total_games = 0;
	
	void updateNewGameScores(int game_score) {
		total_score += game_score;
		if (game_score > best_score) { best_score = game_score; }
		if (game_score < worst_score) { worst_score = game_score; }
		++total_games;
	}
};

// Callback function for select query.
static int select_callback(void* users, int argc, char **argv, char **colName) {
	auto users_vector = (vector<User>*)users;
	User user;
	user.id = std::stoi(argv[0]);
	user.username = argv[1];
	user.password = argv[2];

	user.total_score = std::stoi(argv[3]);
	user.best_score = std::stoi(argv[4]);
	user.worst_score = std::stoi(argv[5]);
	user.total_games = std::stoi(argv[6]);
	users_vector->push_back(user);
	return 0;
}

// Lock which should be held before doing any state change operations.
std::mutex db_lock;

// Class for lock protected database operations.
class Database {
	private:
		Database() {}
	public:
		static bool isLoaded; 
		static sqlite3 *db;
		static vector<User>* users;

		static bool Load() {
			if (isLoaded) {
				return true;
			}

			db_lock.lock();
			int retCode = sqlite3_open(DBNAME, &db);
	
			if(retCode) {
				cout << "Error opening the database, error " <<  sqlite3_errmsg(db) << endl;
				return false;
			}

			char *errorCode = 0;
			string select_query("SELECT * FROM ");
			select_query.append(TABLENAME);
	   		retCode = sqlite3_exec(db, select_query.c_str(), select_callback, (void *)users, &errorCode);
			isLoaded = true;
			db_lock.unlock();
			return true;
		}

		static int searchUserIndex(const string& username) {
			int index = -1;
			for(size_t i=0; i < users->size(); ++i) {
				if ((*users)[i].username.compare(username) == 0) {
					return i;
				}	
			}
			return index;
		}
	
		static bool searchUser(const User& user) {
			for (auto u : *Database::users) {
				if (user.username.compare(u.username) == 0) {
					return true;
				}
			}	
			return false;
		}
	
		// If user exits update the record, otherwise insert the record.
		static bool AddUser(const User& user) {
			db_lock.lock();
			if (!isLoaded) {
				Database::Load();
			}

			if (Database::searchUser(user)) {
				db_lock.unlock();
				return false;
			} else {
				const char *insert_query_template = "INSERT INTO %s ('USERNAME', 'PASSWORD', 'TOTAL_SCORE', 'BEST_SCORE', 'WORST_SCORE', 'TOTAL_GAMES') VALUES(\"%s\", \"%s\",%d, %d, %d, %d)";
				char insert_sql[300];
				sprintf(insert_sql, insert_query_template, TABLENAME, user.username.c_str(), user.password.c_str(), user.total_score, user.best_score, user.worst_score, user.total_games); 
		
				char *errorCode = 0;
				sqlite3_exec(db, insert_sql, NULL, 0, &errorCode);
				Database::users->push_back(user);
				db_lock.unlock();
				return true;
			}
		}

		static bool syncUserToDb(const User& user) {
			db_lock.lock();
			const char *update_sql_template = "UPDATE %s SET PASSWORD = \"%s\", TOTAL_SCORE = %d, BEST_SCORE = %d, WORST_SCORE = %d, TOTAL_GAMES = %d WHERE USERNAME = \"%s\"";
			char update_sql[300];
			sprintf(update_sql, update_sql_template, TABLENAME, user.password.c_str(), user.total_score, user.best_score, user.worst_score, user.total_games, user.username.c_str());
			
			char *errorCode = 0;
			sqlite3_exec(db, update_sql, NULL, 0, &errorCode);
			db_lock.unlock();
			return true;
		}
 
		static bool syncToDb() {
			for(auto& u : *users) {	
				syncUserToDb(u);
			}	
			return true;
		}

		static void updateGameScore(const GamePlayers& game_players, int game_score) {
			int p1_index = Database::searchUserIndex(game_players.player1.first);
			int p2_index = Database::searchUserIndex(game_players.player2.first);
	
			User& user1 = (*Database::users)[p1_index];
			User& user2 = (*Database::users)[p2_index];

			db_lock.lock();

			user1.updateNewGameScores(game_score);
			user2.updateNewGameScores(game_score);

			db_lock.unlock();
		}
};

bool Database::isLoaded = false;
sqlite3* Database::db = NULL;
vector<User>* Database::users = new vector<User>();

// Function to provide current timestamp.
string currentDateTime() {
    time_t     now = time(0);
    struct tm  tstruct;
    char       buf[80];
    tstruct = *localtime(&now);
    strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);
    return buf;
}

// Function to validate the client username and password.
int validateClientUsername(int sd, string& username) {
    char inBuf[BUFSIZE + 1];
    char outBuf[BUFSIZE + 1];
    int isValid = 0;
    int recvLen;
    User user;
    string repassWord;	
    string msg;

	// This is required for testing server using telnet.
    strcpy(outBuf, "Enter the action:username:password combination\n");
    write(sd, outBuf, strlen(outBuf));
    if ((recvLen = read(sd, inBuf, BUFSIZE)) > 0) { 
			inBuf[recvLen] = '\0';        
    }

	std::vector<string> clInput = split(inBuf, ':');
	if (clInput.size() < 3) {
		cout << "No/invalid response sent from client" << endl;
		strcpy(outBuf, INVALIDRESPONSE);
		WRITE_OUT_BUFFER
		return isValid;
	}
	string input = trim(clInput[0]);
	string userName = trim(clInput[1]);
	string passWord = trim(clInput[2]);
	
	cout << "Entered client input:" << input << " Username:" << userName << " Password: " << passWord << endl;
    if (input.compare("1") == 0) {
    	int index = Database::searchUserIndex(userName);

		// New user and user name is not already taken.
	    if (index == -1) {
			user.username = userName;
			user.password = passWord;
			Database::AddUser(user);
			cout << "Registration successfull for new user" << endl;
			input = "2";
		} else {
			isValid = 0;
			cout << "Username already taken\n";
			strcpy(outBuf, USERNAMETAKEN);
			WRITE_OUT_BUFFER
			return isValid;
		}
    }

	// Existing user login.
    if (input.compare("2") == 0) {
		    isValid = 0;
			username.clear();
			username.append(trim(userName));
			int user_index = Database::searchUserIndex(username);
			if (user_index == -1) {
	 			strcpy(outBuf, USERNAMEERROR);
				cout << "USER_NAME_ERROR: Invalid username passed" << endl;
				WRITE_OUT_BUFFER
				isValid = 0;
			} else {
				// Check for already logged in user.
				user = (*(Database::users))[user_index];
				if (active_connections.if_user_alive(username)) {
					cout << username << " is already logged in"	<< endl;
					strcpy(outBuf, DUPLICATEUSER);
					WRITE_OUT_BUFFER
					isValid = 0;
				} else {
					isValid = 1;
				}
			}

		  	// Check for entered password
			if (isValid) {
				string password = trim(passWord);
				if (password.compare(user.password) == 0) {
			    	cout << PASSWORDVALID << " Password not matching with username" << endl;
			    	strcpy(outBuf, SUCCESS);
				} else {
 			    	fprintf(stdout, PASSWORDERROR);
			   		strcpy(outBuf, PASSWORDERROR);
		 			isValid = 0;
				}		
		 			WRITE_OUT_BUFFER		
			}
				return isValid;
		}
	return isValid;
}

// Function to check for valid username and password combination.
int userNameHandler(int fd) {
	cout << "Checking for username and password" << endl;
		
	// Validate username and password for three attempts.
	string username;
	// Make sure if one user is already logged in, don't let him login again.
	if (!validateClientUsername(fd, username)) {
		cout << "Wrong username and password or user already logged in, exiting" << endl;
		if (!pthread_detach(pthread_self())) {
			fprintf(stdout, "Thread detached for the given sock_id: %d \n", fd); 
		}
		close(fd);
	} else {
		active_connections.push_back(username, fd);
		cout << "Success! Valid credentials" << endl;	
		// Send user stats to the socket
		for(size_t i=0; i < Database::users->size(); ++i) {
			if ((*Database::users)[i].username.compare(username) == 0) {
				User user = (*Database::users)[i];
				string user_stats = user.username + "_" + to_string(user.total_score) + "_" + to_string(user.best_score) + "_" + to_string(user.worst_score) + "_";
				write(fd, user_stats.c_str(), user_stats.length());
			}
		}
	}
	return 0;
}

// Read all the words from file.
void readWordsFile() {
	ifstream wordFile(WORDS_FILE_LOC);
	
	cout << "Reading from file started..." + currentDateTime() << endl;
	string line;
	
	while (getline(wordFile, line)) {
		words.push_back(line);
		line.clear();
	}
	
	wordFile.close();
	cout << "Reading done at: " << currentDateTime() << endl;

	if (words.empty()) {
		cerr << "No words found" << endl;
	}	
}

// Function to find random position in list.
int randomPosition(vector<int>& arr) {
  int randomNum;
  bool found = false;
  while (found == false) {
    randomNum = rand() % (words.size());
    if (find(arr.begin(), arr.end(), randomNum) == arr.end()) {
      arr.push_back(randomNum);
      found = true;
    }
  }
  return randomNum;
}

// This function takes care of a game between two selected players.
void* gameHandler(void* game_players) {
	GamePlayers gp = *((GamePlayers *)game_players);
	int fd1 = gp.player1.second;
	int fd2 = gp.player2.second;
	
	string username1 = gp.player1.first;
	string username2 = gp.player2.first;

	// Initiailize game score to be zero.
	int game_score = 0;

	// Variables to make sure new word is selected
  	// correctly and game scores are also correct.
	bool new_word_wanted = true;
	string word = "";
	int word_try_count = 1;
	vector<string> previous_responses;

	string response1, response2;
	char inBuf[BUFSIZE + 1];

	signal(SIGPIPE, SIG_IGN);
	while(1) {
		// These were not functioning correctly, therefore,
		// commenting them out, may explore later if we
		// could leverage these.
		// Check if both the sockets are alive/valid.
		// int errorCode1 = -1, errorCode2 = -1;
		// socklen_t len = sizeof (errorCode1);
		
   		// getsockopt (fd1, SOL_SOCKET, SO_ERROR, &errorCode1, &len) << endl;
		// getsockopt (fd2, SOL_SOCKET, SO_ERROR, &errorCode2, &len) << endl;

		vector<int> arr;
		if (new_word_wanted) {
			word_try_count = 1;
	    	int pos = randomPosition(arr);
      		word = words[pos];
      		cout << "Sent word: " << word << endl;
		} 

		// Send the same word to both the sockets and wait for responses.
		cout << "Going to write to " << "username:fd = " << username1 << ":" << fd1 << endl;
		errno = 0;

		// These messages would be sent to respective clients.
		// Message Structure:
		// <WORD>_<GAME_SCORE>_<OTHER_USERNAME>_<OTHER_USER_PREVIOUS_RESPONSE>
		string message_suffix_1 = to_string(game_score) + "_" + username2 + "_" + response2;
		string message_suffix_2 = to_string(game_score) + "_" + username1 + "_" + response1;

		string message1 = word + "_" + message_suffix_1;
		string message2 = word + "_" + message_suffix_2;

		write(fd1, message1.c_str(), message1.length());
		cout << "Last reponse stats sent to player1:" << message1 << endl;
		// Check for player 1 termination.
		if (errno != 0) {
			message2 = "GAMEEND_" +  message_suffix_2;
			write(fd2, message2.c_str(), message2.length());
			cout << "Notified " << "username:fd = " << username2 << ":" << fd2 << " of game shutdown." << endl;

			// Remove this player from active connections.
			active_connections.search_and_delete(gp.player1);

			// Add the other player to free players list.
			active_connections.add_free_player(gp.player2);

			Database::updateGameScore(gp, game_score);
			break;
		}
		errno = 0;
		cout << "Going to write to " << "username:fd = " << username2 << ":" << fd2 << endl;
		write(fd2, message2.c_str(), message2.length());
		// Check for player 2 termination
		cout << "Last reponse stats sent to player1:" << message1 << endl;
		if (errno != 0) {
			message1 = "GAMEEND_" + message_suffix_1;
			write(fd1, message1.c_str(), message1.length());
			cout << "Notified " << "username:fd = " << username1 << ":" << fd1 << " of game shutdown." << endl;
			
			active_connections.search_and_delete(gp.player2);
			active_connections.add_free_player(gp.player1);

			Database::updateGameScore(gp, game_score);
			break;
		} 

		// Read clients responses	
		int recvLen;
		
		cout << "Going to read from " << "username:fd = " << username1 << ":" << fd1 << endl;
		while((recvLen = read(fd1, inBuf, BUFSIZE)) > 0) {
			inBuf[recvLen] = '\0';
			break;
		}
		response1 = trim(string(inBuf));
		// If client dies, it sends empty string
		if (response1.compare("") == 0) {
			errno = 0;
			write(fd1, "1", 1);
			if (errno != 0) {
				message2 = "GAMEEND_" +  message_suffix_2;
				write(fd2, message2.c_str(), message2.length());
				cout << "Notified " << "username:fd = " << username2 << ":" << fd2 << " of game shutdown." << endl;

				// Remove this player from active connections.
				active_connections.search_and_delete(gp.player1);

				// Add the other player to free players list.
				active_connections.add_free_player(gp.player2);

				Database::updateGameScore(gp, game_score);
				break;
			}
		}
		TO_LOWER(response1);
		cout << "Read from username:fd = " << username1 << ":" << fd1 << " response: " << response1 << endl;
		inBuf[0] = '\0';

		cout << "Going to read from " << "username:fd = " << username2 << ":" << fd2 << endl;
		while((recvLen = read(fd2, inBuf, BUFSIZE)) > 0) {
			inBuf[recvLen] = '\0';
			break;
		}
		response2 = trim(string(inBuf));
		if (response2.compare("") == 0) {
			errno = 0;
			write(fd2, "2", 1);
			if (errno != 0) {
				message1 = "GAMEEND_" + message_suffix_1;
				write(fd1, message1.c_str(), message1.length());
				cout << "Notified " << "username:fd = " << username1 << ":" << fd1 << " of game shutdown." << endl;
			
				active_connections.search_and_delete(gp.player2);
				active_connections.add_free_player(gp.player1);

				Database::updateGameScore(gp, game_score);
				break;
			}	
		}
		TO_LOWER(response2);
		cout << "Read from username:fd = " << username2 << ":" << fd2 << " response: " << response2 << endl;
		inBuf[0] = '\0';

		if (response1.compare(response2) == 0 && response1.compare(NO_RESPONSE) != 0) {
			bool response_match_credit = true;
			// Check if it is a repeated answer.
			for(auto prev_response : previous_responses) {
				if(response1 == prev_response) {
					response_match_credit = false;
					break;	
				}
			}
		
			if (response_match_credit) {
				game_score += (word_try_count++ * 100);
				previous_responses.push_back(response1);
			}
			new_word_wanted = false;
		} else {
			new_word_wanted = true;
			previous_responses.clear();
		}

		cout << "\n*********************\n";
		cout << "Dumping state: " << endl;
		active_connections.dump_state();	
		cout << "State Dump Complete" << endl;
		cout << "\n*********************\n";

		cout << "Game score is " << game_score << endl;
	}
	return NULL;
}

void* syncMemoryToDb(void*) {
	while(1) {
		sleep(DB_REFRESH_RATE);
		cout << "##### Sinking game state to database for all users" << endl;
		Database::syncToDb();
	}
	return 0;
}

/*
 * startNewGames: Main function to start new games between free players.
*/
void * 
startNewGames(void* ) {
  	// Read words file.
  	readWordsFile();
	
	// Seed the random number.
	srand(time(0));

	while(1) {
		if (active_connections.free_players_count() >= 2) {
			pair<string, int> player1 = active_connections.get_free_player();
			pair<string, int> player2 = active_connections.get_free_player();
			
			cout << "Player1: " << player1.first << ":" << player1.second << " and Player2: " << player2.first << ":" << player2.second << endl;
			
			// Start new game for these two players.
			GamePlayers gp;
			gp.player1 = player1;
			gp.player2 = player2;

	    	pthread_t th;
      		if (pthread_create(&th, NULL, (void * (*) (void *))gameHandler, (void *) &gp) < 0) {
				  cout << "This thread creation failed." << endl;
      		}
		}
		sleep(5);
	}
	return 0;
}


// GLOBAL Flag which tells whether this server is master or not.
bool is_master = false;

// Whether a server is high priority of low, depends on what 
// server interaction port is used.
bool is_high_priority_server = false;
void* handleInterServerCommunication(void*) {
	cout << "Started thread to handle messages from other server." << endl;
	string my_server_port;
	if (is_high_priority_server) {
		my_server_port = HIGH_PRIORITY_PORT;
	} else {
		my_server_port = LOW_PRIORITY_PORT;
	}

	int msock = passiveTCP(my_server_port.c_str(), QLEN);
	cout << "Opened listening port for inter server communication" << endl;

	struct sockaddr_in clientAddr;
	socklen_t addr_len;
	
	addr_len = sizeof(clientAddr);
	while(1) {
		cout << "Waiting for another server's connection" << endl;
		int ssock = accept(msock, (struct sockaddr *) &clientAddr, &addr_len);	
	
		if (ssock < 0) {
			if (errno == EINTR) {
				continue;
			}
			errexit("Failure in accepting connections \n");
		}
		if (!is_high_priority_server) {
			char inBuf[BUFSIZE + 1];
			int recvLen;
  	  if ((recvLen = read(ssock, inBuf, BUFSIZE)) > 0) { 
				inBuf[recvLen] = '\0';        
    	}
    	string input = trim(string(inBuf));
			cout << "Received " << input << " from other server" << endl;
		
			if (is_master) {
				write(ssock, "YES", 3);
			} else {
				write(ssock, "NO", 2);
			}
		}
		close(ssock);
	}
	return 0;
}

// This function keeps on checking whether other server is
// up or not.
void* startMasterSlaveCommunication(void* my_server_interaction_port) {
	cout << "Starting master slave communicataion." << endl;
	string interaction_port = string((char*) my_server_interaction_port);
	
	// One of the two servers is high priority and other is lower.
    // This is done just to avoid the race conditions when both
    // servers start at same time and try to become master. 
	// Our approach however, would be:
	// When high priority server starts up:
	// 	1) It checks if low priotiy server is up, if no, become master.
	// 	2) If low priority server is up, then check whether low priority is master already.
	//     If low priority is not yet master, it would become master, otherwise, would just wait for low
    //		 priority one to go down.
	// On the other hand, if low priority one start up:
	//  1) It checks whether, high priority is up, it just waits.
	//  2) If high priotiy is down, it becomes master.
	is_high_priority_server = false;
	if (interaction_port.compare(HIGH_PRIORITY_PORT) == 0) {
		cout << "This is a high priority server." << endl;
		is_high_priority_server = true;
	} else {
		cout << "This is a low priority server." << endl;
	}

 	pthread_attr_t ta;
	pthread_t th;
	
	(void) pthread_attr_init(&ta);
	(void) pthread_attr_setdetachstate(&ta, PTHREAD_CREATE_DETACHED); 	

	// Create server socket for master-slave communication.
	if (pthread_create(&th, &ta, (void * (*) (void *))(handleInterServerCommunication), NULL) < 0) {
	  errexit("Error in creating thread for starting new game.\n");
	}

	int sd;
	if (is_high_priority_server) {
		while(1) {
			// Create socket for communicating with other server port.
			cout << "Trying to connect to " << LOW_PRIORITY_PORT << endl;
			sd = connectTCP("localhost", LOW_PRIORITY_PORT.c_str());
			if (sd == -1) {
				is_master = true;
				close(sd);
				break;
			}
			string are_u_master("ARE_U_MASTER");
			cout << "Sending " << are_u_master << " to " << LOW_PRIORITY_PORT << endl;
			write(sd, are_u_master.c_str(), are_u_master.size());
			char inBuf[BUFSIZE + 1];
			int recvLen;
  	  		if ((recvLen = read(sd, inBuf, BUFSIZE)) > 0) { 
				inBuf[recvLen] = '\0';        
    		}
    		string input = trim(string(inBuf));
			cout << "Received " << input << " from " << LOW_PRIORITY_PORT << endl;
			if (input.compare("NO") == 0) {
				is_master = true;
				close(sd);
				break;
			}
			close(sd);
			sleep(SERVER_COMM_RATE);
		}	
	} else {
		while(1) {
			// Create socket for communicating with other server port.
			cout << "Trying to connect to " << HIGH_PRIORITY_PORT << endl;
			sd = connectTCP("localhost", HIGH_PRIORITY_PORT.c_str());
			if (sd == -1) {
				is_master = true;
			}
			close(sd);
			if(!is_master) {
				sleep(SERVER_COMM_RATE);
			} else {
				break;
			}
		}
	}
	return 0;
}

// Main function to get client's requests, create threads for different operations.
int main(int argc, char *argv[])
{
 	pthread_attr_t ta;
	pthread_t th;
	
	char *service, *server_interaction_port;    //service name or port number
	int msock, ssock;
	
	struct sockaddr_in clientAddr;
	socklen_t addr_len;

	switch (argc) {
		case 3:
			service = argv[1];
			server_interaction_port = argv[2];
			break;
		default:
			errexit("Error in setting server: usage: mind_sync [port] [server_interaction_port]\n");
	}

	cout << "Port for clients: " << service << endl;
	cout << "Port for server interaction: " << server_interaction_port << endl;

	(void) pthread_attr_init(&ta);
	(void) pthread_attr_setdetachstate(&ta, PTHREAD_CREATE_DETACHED); 	

	// Start a thread to keep track the master server, and see if 
    // this server needs to become master now.
	if (pthread_create(&th, &ta, (void * (*) (void *))(startMasterSlaveCommunication), (void*)server_interaction_port) < 0) {
	  errexit("Error in creating thread for starting new game.\n");
	}

	// Keep checking whether this server does not
	// become master. Once a server becomes master,
	// it does not goes back to slave mode.
	while(!is_master) {
		sleep(1);
	}
	
	/* Create listening socket */
	msock = passiveTCP(service, QLEN);

	// Load database.
	Database::Load();
	
	// Start a thread which syncs state to memory every 1 min.	
	if (pthread_create(&th, &ta, (void * (*) (void *))(syncMemoryToDb), NULL) < 0) {
	  errexit("Error in creating thread for starting new game.\n");
	}

	// Start thread which looks at the state of number
	// of active connections. If active connections are 
    // more than, then it spawns out a new thread which
    // runs a game.
	if (pthread_create(&th, &ta, (void * (*) (void *))(startNewGames), NULL) < 0) {
	  errexit("Error in creating thread for starting new game.\n");
	}

	/* Loop to get new connections */
	while(1)
	{		
		// Accepting new request from client.
		addr_len = sizeof(clientAddr);
		ssock = accept(msock, (struct sockaddr *) &clientAddr, &addr_len);	
	
		if (ssock < 0) {
			if (errno == EINTR)
				continue;
			errexit("Failure in accepting connections \n");
	 	}
		
		/* Validate username and passwords */
		if (pthread_create(&th, &ta, (void * (*) (void *))userNameHandler, (void *) ssock) < 0) {
			errexit("Error in creating thread for username validation\n");
		}
	}
}

