// ====================================================================
//  CLIENT.CPP (Cross-Platform - v10 - No Spaces Logic)FINAL CODE
// ====================================================================

#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <cstdio>
#include <ios>
#include <limits> // For numeric_limits

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <cerrno>
    #define SOCKET int
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define closesocket(s) close(s)
    #define WSAGetLastError() errno
    #define WSAStartup(w, d) (0)
    #define WSACleanup()
    struct WSADATA { int wVersion; };
    #define MAKEWORD(a, b) (0)
#endif

using namespace std;

string sendRequest(SOCKET sock, const string& msg) {
    int sendResult = send(sock, msg.c_str(), msg.length(), 0);
    if (sendResult == SOCKET_ERROR) return "ERR Connection lost (send failed)";
    char buffer[8192];
    int bytesReceived = recv(sock, buffer, sizeof(buffer)-1, 0);
    if (bytesReceived <= 0) return "ERR Connection lost (recv failed)";
    buffer[bytesReceived] = '\0';
    return string(buffer);
}

// Helper to clear the input buffer if user types garbage
void clearInputBuffer() {
    cin.clear();
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

void loggedInMenu(SOCKET sock) {
    int choice;
    do {
        cout << "\n--- Main Menu ---" << endl;
        cout << "1. Deposit" << endl;
        cout << "2. Withdraw" << endl;
        cout << "3. Transfer" << endl;
        cout << "4. Check Balance" << endl;
        cout << "5. View Statement" << endl;
        cout << "6. View Profile" << endl;
        cout << "7. Edit Profile" << endl;
        cout << "8. Add Credit Card" << endl;
        cout << "9. Show Credit Cards" << endl;
        cout << "10. Add Debit Card" << endl;
        cout << "11. Show Debit Cards" << endl;
        cout << "12. Logout" << endl;
        cout << "Enter choice: " << flush;

        if (!(cin >> choice)) {
            cout << "❌ Invalid input! Please enter a number." << endl;
            clearInputBuffer();
            continue;
        }

        string request = "";
        string response = "";

        if (choice == 1) { // Deposit
            double amt;
            cout << "Enter amount to deposit: " << flush;
            cin >> amt;
            request = "DEPOSIT|" + to_string(amt) + "|";
        }
        else if (choice == 2) { // Withdraw
            double amt;
            cout << "Enter amount to withdraw: " << flush;
            cin >> amt;
            request = "WITHDRAW|" + to_string(amt) + "|";
        }
        else if (choice == 3) { // Transfer
            string toAcc; int sourceChoice; double amt;

            string sourceMenu = sendRequest(sock, "GET_SOURCES|\n");
            stringstream ss_menu(sourceMenu);
            string menu_status, menu_count_str;
            getline(ss_menu, menu_status, ' ');
            getline(ss_menu, menu_count_str);

            cout << "\n  Select transfer source:" << endl;
            cout << ss_menu.rdbuf();

            cout << "  Enter choice: " << flush;
            cin >> sourceChoice;

            // Standard cin is fine since Account Num has NO spaces
            cout << "Enter target account no: " << flush;
            cin >> toAcc;

            cout << "Enter amount: " << flush;
            cin >> amt;

            request = "TRANSFER|" + toAcc + "|" + to_string(amt) + "|" + to_string(sourceChoice) + "|";
        }
        else if (choice == 4) { // Check Balance
            request = "BALANCE|";
        }
        else if (choice == 5) { // View Statement
            request = "GET_STATEMENT|";
        }
        else if (choice == 6) { // View Profile
            request = "GET_PROFILE|";
        }
        else if (choice == 7) { // Edit Profile
            string n, p, e, a;
            // Use getline for these because names/addresses HAVE spaces
            cout << "Enter new name: " << flush; cin >> ws; getline(cin, n);
            cout << "Enter new phone: " << flush; cin >> ws; getline(cin, p);
            cout << "Enter new email: " << flush; cin >> ws; getline(cin, e);
            cout << "Enter new address: " << flush; cin >> ws; getline(cin, a);
            request = "EDIT_PROFILE|" + n + "|" + p + "|" + e + "|" + a + "|";
        }
        else if (choice == 8) { // Add Credit Card
            double limit;
            cout << "Enter Credit Limit: " << flush;
            cin >> limit;
            request = "ADD_CREDIT_CARD|" + to_string(limit) + "|";
        }
        else if (choice == 9) { // Show Credit Cards
            request = "GET_CREDIT_CARDS|";
        }
        else if (choice == 10) { // Add Debit Card
            request = "ADD_DEBIT_CARD|";
        }
        else if (choice == 11) { // Show Debit Cards
            request = "GET_DEBIT_CARDS|";
        }
        else if (choice == 12) { // Logout
            request = "LOGOUT|";
        }
        else {
            cout << "❌ Invalid choice." << endl;
            continue;
        }

        response = sendRequest(sock, request + "\n");
        cout << "\n[Server Response]\n" << response << endl;

        if (choice == 12) break;

    } while (choice != 12);
}

int main() {
    cout << unitbuf; // Force flush for Mac

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return 1;

    string serverIP;
    int serverPort;
    cout << "Enter Server IP (e.g., 127.0.0.1): " << flush;
    cin >> serverIP;
    cout << "Enter Server Port (e.g., 8080): " << flush;
    cin >> serverPort;

    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == INVALID_SOCKET) return 1;

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);
    serverAddr.sin_addr.s_addr = inet_addr(serverIP.c_str());

    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cout << "❌ Could not connect to the server." << endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    cout << "✅ Connected to bank server!" << endl;

    int choice;
    do {
        cout << "\n🏦 Welcome to the Banking System 🏦" << endl;
        cout << "1. New Customer" << endl;
        cout << "2. Existing Customer" << endl;
        cout << "3. Quit" << endl;
        cout << "Enter your choice: " << flush;

        if (!(cin >> choice)) {
            cout << "❌ Invalid input!" << endl;
            clearInputBuffer();
            continue;
        }

        if (choice == 1) { // New Customer
            string name, pass; double initial;
            cout << "Enter your name: " << flush; cin >> ws; getline(cin, name);
            cout << "Enter initial deposit: Rs." << flush; cin >> initial;
            cout << "Create a password: " << flush; cin >> ws; getline(cin, pass);

            string request = "CREATE|" + name + "|" + to_string(initial) + "|" + pass + "|\n";
            string response = sendRequest(clientSocket, request);
            cout << "\n[Server Response]\n" << response << endl;
        }
        else if (choice == 2) { // Existing Customer
            string accNo, pass;
            cout << "Enter account number: " << flush; cin >> accNo;
            cout << "Enter password: " << flush; cin >> ws; getline(cin, pass);

            string request = "LOGIN|" + accNo + "|" + pass + "|\n";
            string response = sendRequest(clientSocket, request);
            cout << "\n[Server Response]\n" << response << endl;

            if (response.rfind("OK", 0) == 0) loggedInMenu(clientSocket);
        }
        else if (choice == 3) cout << "👋 Thank you!" << endl;
        else cout << "❌ Invalid choice!" << endl;
    } while (choice != 3);

    closesocket(clientSocket);
    WSACleanup();
    return 0;
}
