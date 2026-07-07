#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <fstream>
#include <thread>
#include <mutex>
#include <iomanip>
#include <ctime>
#include <memory>
#include <chrono> 
#include <limits>

using namespace std;

mutex coutMutex;

// ===================== CURRENT TIMES =================
string getCurrentTime() {
    static mutex timeMutex;
    lock_guard<mutex> lock(timeMutex);

    time_t now = time(0);
    tm* ltm = localtime(&now);

    char buffer[9];
    sprintf(buffer, "%02d:%02d:%02d",
            ltm->tm_hour,
            ltm->tm_min,
            ltm->tm_sec);

    return string(buffer);
}

// ================= INPUT VALIDATION HELPER =================
void clearInput() {
    cin.clear();
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

// ================= CURRENCY =================
enum Currency { VND, USD, EUR };

string currencyToString(Currency c) {
    if (c == VND) return "VND";
    if (c == USD) return "USD";
    return "EUR";
}

Currency inputCurrency() {
    int c;
    cout << "1. VND  2. USD  3. EUR\nChoose: ";
    if (!(cin >> c)) {
        clearInput();
        return VND;
    }
    if (c == 2) return USD;
    if (c == 3) return EUR;
    return VND;
}

// ================= EXCHANGE RATE =================
class ExchangeRate {
public:
    static constexpr double USD_TO_VND = 26000.0;
    static constexpr double EUR_TO_VND = 30000.0;
    static constexpr double USD_TO_EUR = 0.9;
    static constexpr double EUR_TO_USD = 1.1;

    static double getRate(Currency from, Currency to) {
        if (from == to) return 1;

        if (from == USD && to == VND) return USD_TO_VND;
        if (from == VND && to == USD) return 1.0 / USD_TO_VND;

        if (from == EUR && to == VND) return EUR_TO_VND;
        if (from == VND && to == EUR) return 1.0 / EUR_TO_VND;

        if (from == USD && to == EUR) return USD_TO_EUR;
        if (from == EUR && to == USD) return EUR_TO_USD;

        return -1;
    }
};

// ================= ACCOUNT =================
class Account {
friend class Transaction;
protected:
    int id;
    double balance;
    mutable mutex mtx;
    Currency currency;

    // Unlocked helper methods to ensure thread-safety and avoid deadlocks in transfers
    virtual void deposit_unlocked(double amount) {
        if (amount > 0) balance += amount;
    }

    virtual bool withdraw_unlocked(double amount) {
        if (amount <= balance) {
            balance -= amount;
            return true;
        }
        return false;
    }

public:
    Account(int id, double bal, Currency cur)
        : id(id), balance(bal), currency(cur) {}

    virtual ~Account() = default; // Virtual destructor to prevent Undefined Behavior on polymorphic deletes

    virtual void deposit(double amount) {
        lock_guard<mutex> lock(mtx);
        deposit_unlocked(amount);
    }

    virtual bool withdraw(double amount) {
        lock_guard<mutex> lock(mtx);
        return withdraw_unlocked(amount);
    }

    virtual void display() {
        cout << "ID: " << id << " | Balance: " << balance
             << " " << currencyToString(currency) << endl;
    }

    int getID() const { return id; }
    double getBalance() {
        lock_guard<mutex> lock(mtx);
        return balance;
    }
    Currency getCurrency() const { return currency; }

    virtual string getType() { return "Account"; }
    
    // ================= STEP-BY-STEP WITH/NO LOCK =================
    void withdrawStepNoLock(double amount) {
        {
            lock_guard<mutex> lock(coutMutex);
            cout << "\n===== THREAD " << this_thread::get_id() << " =====\n";
            cout << "[STEP 1] READ balance = " << balance << endl;
        }

        this_thread::sleep_for(chrono::milliseconds(200));

        if (amount <= balance) {
            {
                lock_guard<mutex> lock(coutMutex);
                cout << "[STEP 2] CHECK: enough !!!\n";
            }

            this_thread::sleep_for(chrono::milliseconds(200));

            // ✅ LƯU GIÁ TRỊ RIÊNG
            double newBalance = balance - amount;
            balance = newBalance;

            {
                lock_guard<mutex> lock(coutMutex);
                cout << "[STEP 3] WRITE new balance = " << newBalance << endl;
                cout << "--------------------------------\n";
            }
        } else {
            lock_guard<mutex> lock(coutMutex);
            cout << "[STEP 2] CHECK: failed XXX \n";
        }
    }

    // ================= STEP-BY-STEP WITH LOCK =================
    void withdrawStepLock(double amount) {
        lock_guard<mutex> lock(mtx);

        {
            lock_guard<mutex> lock2(coutMutex);
            cout << "\n******** LOCKED THREAD ********\n";
            cout << "Thread " << this_thread::get_id() << endl;
            cout << "[STEP 1] READ balance = " << balance << endl;
        }

        this_thread::sleep_for(chrono::milliseconds(200));

        if (amount <= balance) {
            balance -= amount;

            {
                lock_guard<mutex> lock2(coutMutex);
                cout << "[STEP 2] SUCCESS !!!\n";
                cout << "[STEP 3] WRITE balance = " << balance << endl;
                cout << "********************************\n";
            }
        } else {
            {
                lock_guard<mutex> lock2(coutMutex);
                cout << "[STEP 2] FAIL XXX \n";
                cout << "********************************\n";
            }
        }
    }
};

// ================= SAVINGS =================
class Savings : public Account {
protected:
    bool withdraw_unlocked(double amount) override {
        if (amount <= 0) {
            cout << "Invalid amount!\n";
            return false;
        }

        if (amount > balance) {
            cout << "Insufficient balance! Cannot withdraw.\n";
            return false;
        }

        balance -= amount;
        return true;
    }

public:
    Savings(int id, double bal, Currency cur)
        : Account(id, bal, cur) {}

    string getType() override { return "Savings"; }
   
    void display() override {
        cout << fixed << setprecision(0);
        cout << "[Savings] ID: " << id
             << " | Balance: " << balance
             << " " << currencyToString(currency) << endl;
    }
};

// ================= CHECKING =================
class Checking : public Account {
    double overdraft;
    double getDefaultOverdraft(Currency cur) {
        if (cur == VND) return 5000000;
        if (cur == USD) return 200;
        if (cur == EUR) return 200;
        return 0;
    }
protected:
    bool withdraw_unlocked(double amount) override {
        if (amount <= 0) {
            cout << "Invalid amount!\n";
            return false;
        }

        if (balance >= amount) {
            // normal withdrawal
            balance -= amount;
            return true;
        }
        else if (balance + overdraft >= amount) {
            // using overdraft
            cout << "Using overdraft!\n";
            balance -= amount;
            return true;
        }
        else {
            // overdraft limit exceeded
            cout << "Overdraft limit exceeded! Transaction failed.\n";
            return false;
        }
    }

public:
    Checking(int id, double bal, Currency cur)
        : Account(id, bal, cur) {
        overdraft = getDefaultOverdraft(cur);
    }

    string getType() override { return "Checking"; }

    void display() override {
        cout << fixed << setprecision(0);
        cout << "[Checking] ";
        Account::display();
    }
};

// ================= TRANSACTION =================
class Transaction {
public:
    static vector<pair<int,string>> history;
    static mutex logMutex;

    static void addLog(int id, const string& log) {
        lock_guard<mutex> lock(logMutex);
        history.push_back({id, log});
    }

    static bool transfer(Account* from, Account* to, double amount) {
        if (!from || !to || amount <= 0) return false;

        double rate = ExchangeRate::getRate(from->getCurrency(), to->getCurrency());
        if (rate <= 0) {
            cout << "Invalid transaction!\n";
            return false;
        }

        // ✅ lock both accounts atomically to prevent deadlocks
        scoped_lock lock(from->mtx, to->mtx);

        double converted = amount * rate;

        // ✅ Perform withdrawal polymorphically without nested locks
        if (!from->withdraw_unlocked(amount)) {
            return false;
        }

        // ✅ Perform deposit
        to->deposit_unlocked(converted);

        // ✅ LOG thread-safe
        string log1 = "[" + getCurrentTime() + "] Sent " +
            to_string((long long)amount) + " " +
            currencyToString(from->getCurrency()) +
            " -> ID " + to_string(to->getID());

        string log2 = "[" + getCurrentTime() + "] Received " +
            to_string((long long)converted) + " " +
            currencyToString(to->getCurrency()) +
            " <- ID " + to_string(from->getID());

        addLog(from->getID(), log1);
        addLog(to->getID(), log2);

        return true;
    }

    static void showHistoryByID(int id) {
        lock_guard<mutex> lock(logMutex); // Protect reading history as well
        cout << "\n=== History of ID " << id << " ===\n";

        bool found = false;
        for (auto& p : history) {
            if (p.first == id){
                cout << p.second << endl;
                found = true;
            }
        }
        if (!found)
            cout << "No transactions found.\n";
    }
};

vector<pair<int, string>> Transaction::history;
mutex Transaction::logMutex;

// ================= FILE =================
void saveToFile(const map<int, unique_ptr<Account>>& accs) {
    ofstream file("data.txt");

    for (auto& p : accs) {
        file << p.second->getType() << " "
             << p.second->getID() << " "
             << p.second->getBalance() << " "
             << p.second->getCurrency() << endl;
    }

    cout << "Saved Successfully!\n";
}

void loadFromFile(map<int, unique_ptr<Account>>& accs) {
    ifstream file("data.txt");

    if (!file) {
        cout << " Cannot open file!\n";
        return;
    }
    accs.clear(); // Unique pointers automatically deallocated
    string type;
    int id, curInt;
    double bal;

    while (file >> type >> id >> bal >> curInt) {
        Currency cur = static_cast<Currency>(curInt);

        if (type == "Savings")
            accs[id] = make_unique<Savings> (id, bal, cur);
        else
            accs[id] = make_unique<Checking> (id, bal, cur);
    }

    cout << "Loaded Successfully !\n";
}

void exportHistoryToFile() {
    ofstream file("log.txt");

    if (!file) {
        cout << " Cannot open file!\n";
        return;
    }
    lock_guard<mutex> lock(Transaction::logMutex);
    for (auto& p : Transaction::history) {
        file << "ID " << p.first << ": " << p.second << endl;
    }

    file.close();
    cout << " Log exported to log.txt\n";
}

// ================= THREAD DEMO =================
void demoStepNoMutex(Account* acc) {
    double amt = acc->getBalance() * 0.7;
    
    thread t1([&](){ acc->withdrawStepNoLock(amt); });
    thread t2([&](){ acc->withdrawStepNoLock(amt); });

    t1.join();
    t2.join();
}

void demoStepMutex(Account* acc) {
    double amt = acc->getBalance() * 0.7;

    thread t1([&](){ acc->withdrawStepLock(amt); });
    thread t2([&](){ acc->withdrawStepLock(amt); });

    t1.join();
    t2.join();
}

// ================= MENU =================
void mainMenu() {
    cout << "\n===== BANK SYSTEM =====\n";
    cout << "1. Account Management\n";
    cout << "2. Transaction\n";
    cout << "3. File System\n";
    cout << "4. Reports\n";
    cout << "5. System Demo\n";
    cout << "0. Exit\n";
    cout << "Choose: ";
}

void accountMenu() {
    cout << "\n=== ACCOUNT MENU ===\n";
    cout << "1. Create account\n";
    cout << "2. Display accounts\n";
    cout << "0. Back\n";
    cout << "Choose: ";
}

void transactionMenu() {
    cout << "\n=== TRANSACTION MENU ===\n";
    cout << "1. Deposit\n";
    cout << "2. Withdraw\n";
    cout << "3. Transfer\n";
    cout << "0. Back\n";
    cout << "Choose: ";
}

void fileMenu() {
    cout << "\n=== FILE MENU ===\n";
    cout << "1. Save to file\n";
    cout << "2. Load from file\n";
    cout << "0. Back\n";
    cout << "Choose: ";
}

void reportMenu() {
    cout << "\n=== REPORT MENU ===\n";
    cout << "1. Show transaction history by ID\n";
    cout << "2. Export history to file\n";
    cout << "0. Back\n";
    cout << "Choose: ";
}

void demoMenu() {
    cout << "\n=== SYSTEM DEMO ===\n";
    cout << "1. Step-by-step (NO mutex)\n";
    cout << "2. Step-by-step (WITH mutex)\n";
    cout << "0. Back\n";
    cout << "Choose: ";
}

// ================= MAIN =================
int main() {
    map<int, unique_ptr<Account>> accounts;
    int choice;

    do {
        mainMenu();
        if (!(cin >> choice)) {
            cout << "Invalid input! Please enter a number.\n";
            clearInput();
            choice = -1; // reset choice to continue the loop
            continue;
        }

        // ================= ACCOUNT =================
        if (choice == 1) {
            int sub;
            do {
                accountMenu();
                if (!(cin >> sub)) {
                    cout << "Invalid input!\n";
                    clearInput();
                    sub = -1;
                    continue;
                }

                if (sub == 1) {
                    int id, type;
                    double bal;

                    cout << "ID: "; 
                    if (!(cin >> id)) {
                        cout << "Invalid ID!\n";
                        clearInput();
                        continue;
                    }
                    cout << "Balance: "; 
                    if (!(cin >> bal)) {
                        cout << "Invalid balance!\n";
                        clearInput();
                        continue;
                    }
                    
                    if (bal < 0) {
                        cout << "Invalid balance!\n";
                        continue;
                    }

                    Currency cur = inputCurrency();

                    cout << "1. Savings  2. Checking: ";
                    if (!(cin >> type)) {
                        cout << "Invalid choice!\n";
                        clearInput();
                        continue;
                    }

                    if (accounts.count(id)) {
                        cout << "Account with ID " << id << " already exists! Overwriting...\n";
                        // Assignment to unique_ptr automatically deallocates the old object
                    }

                    if (type == 1)
                        accounts[id] = make_unique<Savings>(id, bal, cur);
                    else
                        accounts[id] = make_unique<Checking>(id, bal, cur);
                }
                else if (sub == 2) {
                    for (auto& p : accounts)
                        p.second->display();
                }

            } while (sub != 0);
        }

        // ================= TRANSACTION =================
        else if (choice == 2) {
            int sub;
            do {
                transactionMenu();
                if (!(cin >> sub)) {
                    cout << "Invalid input!\n";
                    clearInput();
                    sub = -1;
                    continue;
                }

                // ================= DEPOSIT =================
                if (sub == 1) {
                    int id;
                    double amt;

                    cout << "Enter account ID: ";
                    if (!(cin >> id)) {
                        cout << "Invalid ID!\n";
                        clearInput();
                        continue;
                    }

                    if (!accounts.count(id)) {
                        cout << " Account not found!\n";
                        continue;
                    }

                    cout << "Enter amount (" 
                         << currencyToString(accounts[id]->getCurrency()) << "): ";
                    if (!(cin >> amt)) {
                        cout << "Invalid amount!\n";
                        clearInput();
                        continue;
                    }
                    if (amt <= 0) {
                        cout << "Invalid amount!\n";
                        continue;
                    }

                    accounts[id]->deposit(amt);
                    
                    string log = "[DEPOSIT] +" + to_string(amt) + " " +
                     currencyToString(accounts[id]->getCurrency());

                    Transaction::addLog(id, log);

                    cout << " Deposit successful!\n";
                    cout << " New balance: " ;
                    accounts[id]->display();
                }
                // ================= WITHDRAW =================
                else if (sub == 2) {
                    int id;
                    double amt;

                    cout << "Enter account ID: ";
                    if (!(cin >> id)) {
                        cout << "Invalid ID!\n";
                        clearInput();
                        continue;
                    }

                    if (!accounts.count(id)) {
                        cout << " Account not found!\n";
                        continue;
                    }

                    cout << "Enter amount (" 
                         << currencyToString(accounts[id]->getCurrency()) << "): ";
                    if (!(cin >> amt)) {
                        cout << "Invalid amount!\n";
                        clearInput();
                        continue;
                    }

                    if (amt <= 0) {
                        cout << "Invalid amount!\n";
                        continue;
                    }

                    if (accounts[id]->withdraw(amt)){
                        string log = "[WITHDRAW] -" + to_string(amt) + " " +
                     currencyToString(accounts[id]->getCurrency());
                        Transaction::addLog(id, log);

                        cout << " Withdraw successful!\n";    
                        cout << " New balance: " ;
                        accounts[id]->display();
                    }
                    else {
                        cout << " Withdraw failed!\n";
                    }
                }
                // ================= TRANSFER =================
                else if (sub == 3) {
                    int from, to;
                    double amt;

                    cout << "From account ID: ";
                    if (!(cin >> from)) {
                        cout << "Invalid ID!\n";
                        clearInput();
                        continue;
                    }

                    cout << "To account ID: ";
                    if (!(cin >> to)) {
                        cout << "Invalid ID!\n";
                        clearInput();
                        continue;
                    }

                    if (!accounts.count(from) || !accounts.count(to)) {
                        cout << " Invalid account ID!\n";
                        continue;
                    }

                    cout << "Enter amount (" 
                         << currencyToString(accounts[from]->getCurrency()) << "): ";
                    if (!(cin >> amt)) {
                        cout << "Invalid amount!\n";
                        clearInput();
                        continue;
                    }
                    if (amt <= 0) {
                        cout << "Invalid amount!\n";
                        continue;
                    }

                    cout << "\n Transferring " << amt << " "
                         << currencyToString(accounts[from]->getCurrency())
                         << " from ID " << from
                         << " to ID " << to << endl;

                    if (Transaction::transfer(accounts[from].get(), accounts[to].get(), amt)) {
                        cout << " Transfer successful!\n";
                                        
                        cout << "\nSender new balance:\n";
                        accounts[from]->display();

                        cout << "Receiver new balance:\n";
                        accounts[to]->display(); 
                    }
                    else {
                        cout << " Transfer failed!\n";
                    }
                }

            } while (sub != 0);
        }

        // ================= FILE =================
        else if (choice == 3) {
            int sub;
            do {
                fileMenu();
                if (!(cin >> sub)) {
                    cout << "Invalid input!\n";
                    clearInput();
                    sub = -1;
                    continue;
                }

                if (sub == 1) saveToFile(accounts);
                else if (sub == 2) loadFromFile(accounts);

            } while (sub != 0);
        }

        // ================= REPORT =================
        else if (choice == 4) {
            int sub;
            do {
                reportMenu();
                if (!(cin >> sub)) {
                    cout << "Invalid input!\n";
                    clearInput();
                    sub = -1;
                    continue;
                }

                if (sub == 1) {
                    int id;
                    cout << "Enter ID: ";
                    if (!(cin >> id)) {
                        cout << "Invalid ID!\n";
                        clearInput();
                        continue;
                    }

                    Transaction::showHistoryByID(id);
                }
                else if (sub == 2) {
                    exportHistoryToFile();
                }

            } while (sub != 0);
        }

        // ================= DEMO =================
        else if (choice == 5) {
            int sub;
            do {
                demoMenu();
                if (!(cin >> sub)) {
                    cout << "Invalid input!\n";
                    clearInput();
                    sub = -1;
                    continue;
                }

                if (!accounts.empty()) {
                    auto acc = accounts.begin()->second.get();

                    // reset account for clear demo
                    cout << "\nReset account to 1000 for demo\n";
                    acc->deposit(1000 - acc->getBalance());

                    if (sub == 1) {
                        cout << "\n===== STEP DEMO: NO MUTEX =====\n";
                        cout << "Before: ";
                        acc->display();

                        demoStepNoMutex(acc);

                        cout << "After: ";
                        acc->display();
                    }
                    else if (sub == 2) {
                        cout << "\n===== STEP DEMO: WITH MUTEX =====\n";
                        cout << "Before: ";
                        acc->display();

                        demoStepMutex(acc);

                        cout << "After: ";
                        acc->display();
                    }
                }

            } while (sub != 0);
        }

    } while (choice != 0);

    return 0;
}
