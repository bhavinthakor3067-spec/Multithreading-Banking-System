//FINAL SERVER
// ====================================================================
//  SERVER_V9_VALIDATED.CPP (Cross-Platform - v9 - 12-digit account, 6-char password, 10-digit phone)
//  - Option1: Separate transactionStatements and systemLogs
//  - Account IDs are strings (12-digit)
//  - Password must be exactly 6 characters on CREATE
//  - Phone must be exactly 10 digits on EDIT_PROFILE
// ====================================================================
// ====================================================================
//  SERVER.CPP (Cross-Platform - Phone Validation & Profile Fix)
// ====================================================================

// ====================================================================
//  SERVER.CPP (Cross-Platform - v8 - 12-Digit Acc & Clean History)
// ====================================================================

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <random>
#include <chrono>
#include <thread>
#include <ctime>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>

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

const string ACCOUNTS_FILE = "accounts.dat";
const int SERVER_PORT = 8080;

// ---------- Validation helpers ----------
bool isValidPhone(const string &ph) {
    return ph.size() == 10 && all_of(ph.begin(), ph.end(), ::isdigit);
}

// ---------- Misc helpers ----------
string generateCardNumber() {
    static random_device rd;
    static mt19937 gen(rd());
    uniform_int_distribution<> dist(0, 9);
    stringstream ss;
    for (int i = 0; i < 12; ++i) ss << dist(gen);
    string num = ss.str();
    return num.substr(0, 4) + " " + num.substr(4, 4) + " " + num.substr(8, 4);
}

string currentTime() {
    time_t now = time(nullptr);
    char buf[50];
    strftime(buf, sizeof(buf), "%d-%m-%Y %H:%M:%S", localtime(&now));
    return buf;
}

struct CreditCard {
    string cardNumber;
    double limitAmount;
    double usedAmount;
    CreditCard(string num, double lim) : cardNumber(num), limitAmount(lim), usedAmount(0) {}
    CreditCard(string num, double lim, double used) : cardNumber(num), limitAmount(lim), usedAmount(used) {}
};

struct DebitCard {
    string cardNumber;
    DebitCard(string num) : cardNumber(num) {}
};

// ====================================================================
//  BANK ACCOUNT CLASS (Now uses string accountNumber)
// ====================================================================
class BankAccount {
    string accountNumber; // 12-digit string
    string name, phone, email, address;
    double balance;
    string password;

    // Separated logs
    vector<string> transactionStatements; // money-related: deposit, withdraw, transfer, account-open initial deposit
    vector<string> systemLogs;            // profile changes, card adds, data load, etc.

    vector<CreditCard> creditCards;
    vector<DebitCard> debitCards;
    mutable mutex mtx;

public:
    // New account constructor
    BankAccount(const string &accNo, const string &name, double initialBalance, const string& pass)
        : accountNumber(accNo), name(name), balance(initialBalance), password(pass) {
        phone = "Not Set"; email = "Not Set"; address = "Not Set";
        transactionStatements.push_back("[" + currentTime() + "] 🟢 Account opened with Rs." + to_string(initialBalance));
        systemLogs.push_back("[" + currentTime() + "] 💻 Account created.");
    }

    // Load-from-file constructor
    BankAccount(const string &accNo, string pass, string n, double bal, string ph, string em, string addr,
                  const vector<string>& loadedTransactions, const vector<string>& loadedSystemLogs,
                  const vector<CreditCard>& loadedCreditCards, const vector<DebitCard>& loadedDebitCards)
        : accountNumber(accNo), name(n), phone(ph), email(em), address(addr),
          balance(bal), password(pass), transactionStatements(loadedTransactions),
          systemLogs(loadedSystemLogs), creditCards(loadedCreditCards), debitCards(loadedDebitCards) {
        systemLogs.push_back("[" + currentTime() + "] 💻 Account data loaded into session.");
    }

    string getAccountNumber() const { return accountNumber; }
    string getName() const { return name; }

    bool checkPassword(const string& inputPass) const {
        lock_guard<mutex> lock(mtx);
        return inputPass == password;
    }

    // Save representation — note order: basic fields -> transactions -> system logs -> credit cards -> debit cards
    string getSaveString() const {
        lock_guard<mutex> lock(mtx);
        stringstream ss;
        ss << accountNumber << endl;
        ss << password << endl;
        ss << name << endl;
        ss << fixed << setprecision(2) << balance << endl;
        ss << phone << endl;
        ss << email << endl;
        ss << address << endl;

        // Transactions
        ss << transactionStatements.size() << endl;
        for (const auto& stmt : transactionStatements) ss << stmt << endl;

        // System logs
        ss << systemLogs.size() << endl;
        for (const auto& slog : systemLogs) ss << slog << endl;

        // Credit cards
        ss << creditCards.size() << endl;
        for (const auto& card : creditCards) {
            ss << card.cardNumber << endl << fixed << setprecision(2) << card.limitAmount << endl << fixed << setprecision(2) << card.usedAmount << endl;
        }

        // Debit cards
        ss << debitCards.size() << endl;
        for (const auto& card : debitCards) {
            ss << card.cardNumber << endl;
        }

        string result = ss.str();
        if (!result.empty() && result.back() == '\n') result.pop_back();
        return result;
    }

    // Transaction operations
    string deposit(double amount) {
        if (amount <= 0) return "ERR Deposit amount must be positive!";
        lock_guard<mutex> lock(mtx);
        balance += amount;
        transactionStatements.push_back("[" + currentTime() + "] 💰 Deposited Rs." + to_string(amount) + " | New Balance: Rs." + to_string(balance));
        return "OK New Balance: Rs." + to_string(balance);
    }

    string withdraw(double amount) {
        if (amount <= 0) return "ERR Withdrawal amount must be positive!";
        lock_guard<mutex> lock(mtx);
        if (balance >= amount) {
            balance -= amount;
            transactionStatements.push_back("[" + currentTime() + "] 🏧 Withdrawn Rs." + to_string(amount) + " | New Balance: Rs." + to_string(balance));
            return "OK Withdrawal Successful. New Balance: Rs." + to_string(balance);
        } else {
            systemLogs.push_back("[" + currentTime() + "] ⚠️ Withdrawal failed (Insufficient Funds)");
            return "ERR Insufficient funds!";
        }
    }

    string getBalance() const {
        lock_guard<mutex> lock(mtx);
        return "OK Your balance is Rs." + to_string(balance);
    }

    // Return only transaction statements
    string getStatementString() const {
        lock_guard<mutex> lock(mtx);
        stringstream ss;
        ss << "OK \n📜 Transaction History\n---------------------------------\n";
        for (const auto& stmt : transactionStatements) ss << stmt << "\n";
        ss << "---------------------------------";
        return ss.str();
    }

    // Return only system logs
    string getSystemLogsString() const {
        lock_guard<mutex> lock(mtx);
        if (systemLogs.empty()) return "OK No system logs available.";
        stringstream ss;
        ss << "OK \n🛠️ System Logs\n---------------------------------\n";
        for (const auto& slog : systemLogs) ss << slog << "\n";
        ss << "---------------------------------";
        return ss.str();
    }

    string getProfileString() const {
        lock_guard<mutex> lock(mtx);
        stringstream ss;
        ss << "OK \n👤 PROFILE DETAILS\n---------------------------\n";
        ss << "Account : " << accountNumber << "\n";
        ss << "Name    : " << name << "\n";
        ss << "Phone   : " << phone << "\n";
        ss << "Email   : " << email << "\n";
        ss << "Address : " << address << "\n---------------------------";
        return ss.str();
    }

    string editProfile(string newName, string newPhone, string newEmail, string newAddr) {
        if (!isValidPhone(newPhone)) return string("ERR Phone must be exactly 10 digits and numeric!");
        lock_guard<mutex> lock(mtx);
        name = newName;
        phone = newPhone;
        email = newEmail;
        address = newAddr;
        systemLogs.push_back("[" + currentTime() + "] ✏️ Profile updated.");
        return "OK Profile updated successfully!";
    }

    string addCreditCard(double limit) {
        lock_guard<mutex> lock(mtx);
        string cardNo = generateCardNumber();
        creditCards.emplace_back(cardNo, limit);
        systemLogs.push_back("[" + currentTime() + "] 💳 Credit Card added: " + cardNo);
        return "OK Credit Card Added: " + cardNo;
    }

    string getCreditCardsString() const {
        lock_guard<mutex> lock(mtx);
        if (creditCards.empty()) return "OK No credit cards linked.";
        stringstream ss;
        ss << "OK \n💳 CREDIT CARDS\n---------------------------------\n";
        for (const auto& c : creditCards) {
            ss << "Card No : " << c.cardNumber << " | Limit: Rs." << c.limitAmount << " | Used: Rs." << c.usedAmount << "\n";
        }
        return ss.str();
    }

    string addDebitCard() {
        lock_guard<mutex> lock(mtx);
        string cardNo = generateCardNumber();
        debitCards.emplace_back(cardNo);
        systemLogs.push_back("[" + currentTime() + "] 🟦 Debit Card added: " + cardNo);
        return "OK Debit Card Added: " + cardNo;
    }

    string getDebitCardsString() const {
        lock_guard<mutex> lock(mtx);
        if (debitCards.empty()) return "OK No debit cards linked.";
        stringstream ss;
        ss << "OK \n🟦 DEBIT CARDS\n---------------------------------\n";
        for (const auto& c : debitCards) {
            ss << "Card No : " << c.cardNumber << "\n";
        }
        return ss.str();
    }

    string getTransferSources() {
        lock_guard<mutex> lock(mtx);
        stringstream ss;
        ss << "  1. Account Balance (Debit) - Available: Rs." << balance << "\n";
        int option = 2;
        for (const auto& card : creditCards) {
            double available = card.limitAmount - card.usedAmount;
            ss << "  " << option << ". Credit Card (" << card.cardNumber << ") - Available: Rs." << available << "\n";
            option++;
        }
        return "OK " + to_string(option - 1) + "\n" + ss.str();
    }

    // Transfer: sourceChoice 1 => account balance, >1 => credit card (index = sourceChoice-2)
    string transfer(BankAccount &to, double amount, int sourceChoice) {
        if (amount <= 0) return "ERR Transfer amount must be positive!";
        if (&to == this) return "ERR Cannot transfer to the same account!";

        // Lock both accounts without deadlock
        lock(mtx, to.mtx);
        lock_guard<mutex> lk1(mtx, adopt_lock);
        lock_guard<mutex> lk2(to.mtx, adopt_lock);

        if (sourceChoice == 1) {
            if (balance >= amount) {
                balance -= amount;
                to.balance += amount;
                string stmt = "[" + currentTime() + "] 🔁 Transferred Rs." + to_string(amount) + " to " + to.name + " (from Account)";
                transactionStatements.push_back(stmt);
                to.transactionStatements.push_back("[" + currentTime() + "] 🔁 Received Rs." + to_string(amount) + " from " + name);
                return "OK Transfer Successful! (from Account)";
            } else {
                systemLogs.push_back("[" + currentTime() + "] ⚠️ Transfer failed (Insufficient Funds)");
                return "ERR Insufficient funds!";
            }
        }
        else if (sourceChoice > 1) {
            int cardIndex = sourceChoice - 2;
            if (cardIndex >= static_cast<int>(creditCards.size())) return "ERR Invalid credit card choice.";

            CreditCard& card = creditCards[cardIndex];
            double availableLimit = card.limitAmount - card.usedAmount;

            if (availableLimit >= amount) {
                card.usedAmount += amount;
                to.balance += amount;
                string stmt = "[" + currentTime() + "] 🔁 Transferred Rs." + to_string(amount) + " to " + to.name + " (from Card " + card.cardNumber + ")";
                transactionStatements.push_back(stmt);
                to.transactionStatements.push_back("[" + currentTime() + "] 🔁 Received Rs." + to_string(amount) + " from " + name);
                return "OK Transfer Successful! (from Credit Card " + card.cardNumber + ")";
            } else {
                return "ERR Insufficient limit on card " + card.cardNumber + ".";
            }
        }
        return "ERR Invalid transfer type.";
    }
};

// Global accounts map (key = 12-digit string)
unordered_map<string, shared_ptr<BankAccount>> accounts;
mutex accounts_mtx;

// Account number generator (12-digit)
string generateAccountNumber() {
    static random_device rd;
    static mt19937 gen(rd());
    uniform_int_distribution<> dist(0, 9);
    string candidate;
    lock_guard<mutex> lock(accounts_mtx);
    do {
        stringstream ss;
        for (int i = 0; i < 12; ++i) ss << dist(gen);
        candidate = ss.str();
    } while (accounts.count(candidate));
    return candidate;
}

// Save all accounts to disk
void saveAccounts() {
    ofstream file(ACCOUNTS_FILE);
    if (!file.is_open()) {
        cout << "CRITICAL ERROR: Could not open " << ACCOUNTS_FILE << " for saving!" << endl;
        return;
    }
    lock_guard<mutex> lock(accounts_mtx);
    for (auto const& [accNo, accountPtr] : accounts) {
        file << accountPtr->getSaveString() << endl;
    }
    file.close();
    cout << "💾 [Server] All account data saved.\n";
}

// Load accounts from disk (must match getSaveString order)
void loadAccounts() {
    ifstream file(ACCOUNTS_FILE);
    if (!file.is_open()) {
        cout << "[Server] No existing account data found. Starting fresh." << endl;
        return;
    }
    string line;
    while (getline(file, line)) {
        if (line.empty()) continue;
        try {
            string accNo = line; // 12-digit string
            string pass, name, ph, em, addr;
            double bal;

            getline(file, pass);
            getline(file, name);

            getline(file, line); bal = stod(line);

            getline(file, ph); getline(file, em); getline(file, addr);

            // Transactions
            int transactionCount; vector<string> loadedTransactions;
            getline(file, line); transactionCount = stoi(line);
            for (int i = 0; i < transactionCount; ++i) { getline(file, line); loadedTransactions.push_back(line); }

            // System logs
            int systemLogCount; vector<string> loadedSystemLogs;
            getline(file, line); systemLogCount = stoi(line);
            for (int i = 0; i < systemLogCount; ++i) { getline(file, line); loadedSystemLogs.push_back(line); }

            // Credit cards
            int creditCardCount; vector<CreditCard> loadedCreditCards;
            getline(file, line); creditCardCount = stoi(line);
            for (int i = 0; i < creditCardCount; ++i) {
                string cardNo; double limit, used;
                getline(file, cardNo); getline(file, line); limit = stod(line);
                getline(file, line); used = stod(line);
                loadedCreditCards.emplace_back(cardNo, limit, used);
            }

            // Debit cards
            int debitCardCount; vector<DebitCard> loadedDebitCards;
            getline(file, line); debitCardCount = stoi(line);
            for (int i = 0; i < debitCardCount; ++i) { string cardNo; getline(file, cardNo); loadedDebitCards.emplace_back(cardNo); }

            auto acc = make_shared<BankAccount>(accNo, pass, name, bal, ph, em, addr, loadedTransactions, loadedSystemLogs, loadedCreditCards, loadedDebitCards);
            {
                lock_guard<mutex> lock(accounts_mtx);
                accounts[accNo] = acc;
            }
        } catch (const exception& e) {
            cout << "⚠️ [Server] Error loading an account record: " << e.what() << endl;
        }
    }
    file.close();
    if (accounts.size() > 0) {
        cout << "✅ [Server] " << accounts.size() << " accounts loaded successfully." << endl;
    }
}

// --- CLIENT HANDLER ---
void handle_client(SOCKET clientSocket) {
    cout << "[Server] Client thread started." << endl;

    shared_ptr<BankAccount> loggedInAccount = nullptr;
    char buffer[8192];
    int bytesReceived;

    while ((bytesReceived = recv(clientSocket, buffer, sizeof(buffer)-1, 0)) > 0) {
        buffer[bytesReceived] = '\0';
        string request(buffer);
        string response = "ERR Invalid request\n";

        stringstream ss(request);
        string command;
        getline(ss, command, '|');

        if (command == "LOGIN") {
            string accNoStr, pass;
            getline(ss, accNoStr, '|');
            getline(ss, pass, '|');
            try {
                string accNo = accNoStr;
                lock_guard<mutex> lock(accounts_mtx);
                if (accounts.count(accNo)) {
                    if (accounts[accNo]->checkPassword(pass)) {
                        loggedInAccount = accounts[accNo];
                        response = "OK Welcome, " + loggedInAccount->getName() + "\n";
                    } else {
                        response = "ERR Incorrect password\n";
                    }
                } else {
                    response = "ERR Account not found\n";
                }
            } catch (const exception&) { response = "ERR Invalid account number format\n"; }
        }
        else if (command == "CREATE") {
            // CREATE|<name>|<deposit>|<password>
            string name, depositStr, pass;
            getline(ss, name, '|'); getline(ss, depositStr, '|'); getline(ss, pass, '|');
            try {
                // New Logic: No explicit password validation check in request,
                // but you could add it here if you want rules like "6 chars".
                // Current BankAccount class doesn't enforce it, so we allow any string.

                double initialDeposit = stod(depositStr);
                string newAccNo = generateAccountNumber();
                auto newAccount = make_shared<BankAccount>(newAccNo, name, initialDeposit, pass);
                {
                    lock_guard<mutex> lock(accounts_mtx);
                    accounts[newAccNo] = newAccount;
                }
                saveAccounts();
                response = "OK Account created! Your new Account Number is: " + newAccNo + "\n";
            } catch (const exception&) { response = "ERR Invalid deposit amount format\n"; }
        }
        else if (command == "LOGOUT") {
            loggedInAccount = nullptr; // Just de-select the account
            response = "OK Logged out successfully\n";
        }
        else if (command == "QUIT") {
            response = "OK Goodbye!\n";
            send(clientSocket, response.c_str(), response.length(), 0);
            break;
        }
        else if (loggedInAccount == nullptr) {
            response = "ERR You must be logged in to do that.\n";
        }
        else if (command == "BALANCE") {
            response = loggedInAccount->getBalance() + "\n";
        }
        else if (command == "DEPOSIT") {
            string amountStr; getline(ss, amountStr, '|');
            try {
                double amount = stod(amountStr);
                response = loggedInAccount->deposit(amount) + "\n";
                saveAccounts();
            } catch (const exception&) { response = "ERR Invalid amount\n"; }
        }
        else if (command == "WITHDRAW") {
            string amountStr; getline(ss, amountStr, '|');
            try {
                double amount = stod(amountStr);
                response = loggedInAccount->withdraw(amount) + "\n";
                saveAccounts();
            } catch (const exception&) { response = "ERR Invalid amount\n"; }
        }
        else if (command == "GET_STATEMENT") {
            response = loggedInAccount->getStatementString() + "\n";
        }
        else if (command == "GET_SYSTEM_LOGS") {
            response = loggedInAccount->getSystemLogsString() + "\n";
        }
        else if (command == "GET_PROFILE") {
            response = loggedInAccount->getProfileString() + "\n";
        }
        else if (command == "EDIT_PROFILE") {
            string n, p, e, a;
            getline(ss, n, '|'); getline(ss, p, '|'); getline(ss, e, '|'); getline(ss, a, '|');
            // p is phone — must be validated
            string res = loggedInAccount->editProfile(n, p, e, a);
            response = res + "\n";
            if (res.rfind("OK", 0) == 0) saveAccounts();
        }
        else if (command == "ADD_CREDIT_CARD") {
            string limitStr; getline(ss, limitStr, '|');
            try {
                double limit = stod(limitStr);
                response = loggedInAccount->addCreditCard(limit) + "\n";
                saveAccounts();
            } catch (const exception&) { response = "ERR Invalid limit\n"; }
        }
        else if (command == "GET_CREDIT_CARDS") {
            response = loggedInAccount->getCreditCardsString() + "\n";
        }
        else if (command == "ADD_DEBIT_CARD") {
            response = loggedInAccount->addDebitCard() + "\n";
            saveAccounts();
        }
        else if (command == "GET_DEBIT_CARDS") {
            response = loggedInAccount->getDebitCardsString() + "\n";
        }
        else if (command == "GET_SOURCES") {
            response = loggedInAccount->getTransferSources();
        }
        else if (command == "TRANSFER") {
            string toAccStr, amountStr, sourceStr;
            getline(ss, toAccStr, '|'); getline(ss, amountStr, '|'); getline(ss, sourceStr, '|');
            try {
                string toAcc = toAccStr; // Account number is a string now
                double amount = stod(amountStr);
                int source = stoi(sourceStr);
                shared_ptr<BankAccount> toAccount;
                {
                    lock_guard<mutex> lock(accounts_mtx);
                    if (accounts.count(toAcc)) toAccount = accounts[toAcc];
                }
                if (toAccount) {
                    response = loggedInAccount->transfer(*toAccount, amount, source) + "\n";
                    saveAccounts();
                } else {
                    response = "ERR Recipient account not found\n";
                }
            } catch (const exception&) { response = "ERR Invalid transfer data\n"; }
        }

        send(clientSocket, response.c_str(), response.length(), 0);
    }
    cout << "[Server] Client disconnected." << endl;
    closesocket(clientSocket);
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return 1;
    loadAccounts();
    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket == INVALID_SOCKET) return 1;

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(SERVER_PORT);

    if (bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) return 1;
    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) return 1;

    cout << "✅ Server is running and listening on port " << SERVER_PORT << "..." << endl;

    while (true) {
        sockaddr_in clientAddr;
        #ifdef _WIN32
            int clientAddrSize = sizeof(clientAddr);
        #else
            socklen_t clientAddrSize = sizeof(clientAddr);
        #endif
        SOCKET clientSocket = accept(listenSocket, (sockaddr*)&clientAddr, &clientAddrSize);
        if (clientSocket == INVALID_SOCKET) continue;
        cout << "[Server] New client connected!" << endl;
        thread(handle_client, clientSocket).detach();
    }
    closesocket(listenSocket);
    WSACleanup();
    return 0;
}
