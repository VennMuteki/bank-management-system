# 🏦 Bank Management System (Multi-threaded C++)

Hệ thống quản lý ngân hàng mô phỏng các nghiệp vụ **gửi tiền, rút tiền, chuyển khoản đa tiền tệ** trong môi trường **đa luồng (multi-threading)**, minh họa vấn đề **Race Condition** và cách giải quyết bằng **Mutex / Lock**.

---

## 📌 Giới thiệu

Đồ án xây dựng một hệ thống ngân hàng console mô phỏng với hai loại tài khoản (**Savings**, **Checking**), hỗ trợ 3 loại tiền tệ (VND, USD, EUR), có khả năng:

- Quản lý tài khoản (tạo, hiển thị)
- Thực hiện giao dịch (gửi, rút, chuyển khoản có quy đổi tỷ giá)
- Lưu/đọc dữ liệu ra file
- Xuất báo cáo lịch sử giao dịch
- **Demo trực quan Race Condition**: so sánh việc rút tiền đồng thời từ 2 luồng **không dùng mutex** (dẫn đến sai lệch số dư) và **có dùng mutex** (đảm bảo tính đúng đắn - thread-safe)

## 🎯 Mục tiêu đồ án

Minh họa cách xử lý các trường hợp cụ thể khi nhiều luồng (thread) cùng truy cập và thay đổi một tài nguyên dùng chung (số dư tài khoản), cụ thể là:

1. **Trường hợp lỗi (Race Condition)**: 2 luồng cùng đọc số dư → cùng kiểm tra đủ tiền → cùng ghi đè kết quả → **mất dữ liệu giao dịch**.
2. **Trường hợp đúng (Mutex Lock)**: Dùng `std::mutex` / `lock_guard` / `scoped_lock` để đảm bảo mỗi thời điểm chỉ một luồng được thao tác trên tài khoản, tránh xung đột và deadlock khi chuyển khoản giữa 2 tài khoản.

---

## 🗂️ Cấu trúc thư mục

```
bank-project/
├── src/
│   └── main.cpp        # Toàn bộ mã nguồn chương trình
├── README.md            # Tài liệu hướng dẫn (file này)
└── .gitignore           # Bỏ qua file build & dữ liệu sinh ra khi chạy
```

> 💡 Khi build/chạy chương trình, hệ thống sẽ tự sinh ra `data.txt` (lưu tài khoản) và `log.txt` (lưu lịch sử giao dịch) tại thư mục chạy chương trình.

---

## 🏗️ Kiến trúc & Sơ đồ lớp (Class Design)

| Lớp | Vai trò |
|---|---|
| `Account` (abstract base, có `virtual`) | Lớp cơ sở đa hình cho tài khoản, quản lý `balance`, `mutex` riêng cho từng account |
| `Savings` : `Account` | Tài khoản tiết kiệm — chỉ rút khi đủ số dư |
| `Checking` : `Account` | Tài khoản thanh toán — hỗ trợ **thấu chi (overdraft)** theo từng loại tiền tệ |
| `ExchangeRate` | Lớp tiện ích static, quy đổi tỷ giá giữa VND/USD/EUR |
| `Transaction` | Lớp static quản lý giao dịch chuyển khoản (`transfer`) và lịch sử log (thread-safe bằng `logMutex`) |

**Nguyên tắc thiết kế nổi bật:**
- **Kế thừa & đa hình (`virtual` + `override`)**: `withdraw_unlocked()` được ghi đè riêng ở `Savings`/`Checking` để áp dụng luật rút tiền khác nhau, nhưng vẫn dùng chung interface `Account`.
- **Encapsulation**: `balance`, `mtx` là `protected`, chỉ thao tác qua các hàm public an toàn luồng.
- **RAII cho khóa**: dùng `lock_guard`/`scoped_lock` thay vì `lock()/unlock()` thủ công để tránh quên mở khóa khi có exception.
- **Tránh deadlock khi transfer**: dùng `std::scoped_lock lock(from->mtx, to->mtx);` để khóa đồng thời 2 mutex theo thứ tự an toàn (deadlock-avoidance algorithm có sẵn của STL) thay vì khóa tuần tự từng cái.
- **`unique_ptr`** quản lý vòng đời tài khoản trong `map<int, unique_ptr<Account>>` → tự động giải phóng bộ nhớ, không rò rỉ.

---

## ⚙️ Yêu cầu môi trường

- Trình biên dịch hỗ trợ **C++11** trở lên (khuyến nghị C++17)
- Hỗ trợ thư viện `<thread>` → cần liên kết pthread trên Linux/macOS

## 🔧 Cách biên dịch & chạy

### Windows (MinGW / g++)
```bash
g++ -std=c++17 -pthread src/main.cpp -o bank.exe
bank.exe
```

### Linux / macOS
```bash
g++ -std=c++17 -pthread src/main.cpp -o bank
./bank
```

### Dùng CMake (tuỳ chọn)
```bash
mkdir build && cd build
cmake .. -DCMAKE_CXX_STANDARD=17
cmake --build .
```

---

## 🖥️ Hướng dẫn sử dụng (Menu chương trình)

```
===== BANK SYSTEM =====
1. Account Management     → Tạo / hiển thị tài khoản
2. Transaction             → Gửi tiền / Rút tiền / Chuyển khoản
3. File System              → Lưu / Đọc dữ liệu từ data.txt
4. Reports                   → Xem lịch sử theo ID / Xuất log ra file
5. System Demo               → Demo Race Condition (No Mutex vs With Mutex)
0. Exit
```

### 👉 Demo Race Condition (quan trọng nhất của đồ án)

1. Vào menu **1 → 1** tạo ít nhất 1 tài khoản.
2. Vào menu **5 (System Demo)**:
   - Chọn **1 (NO mutex)**: chương trình tạo 2 luồng cùng rút tiền từ 1 tài khoản, in ra từng bước READ → CHECK → WRITE. Do không khóa, hai luồng có thể cùng đọc số dư cũ trước khi bên kia ghi → **số dư cuối SAI** (không trừ đủ tổng 2 lần rút).
   - Chọn **2 (WITH mutex)**: lặp lại thao tác tương tự nhưng có `lock_guard(mtx)` bọc toàn bộ quy trình → **số dư cuối ĐÚNG**, minh chứng khóa mutex đảm bảo tính nhất quán dữ liệu.
3. So sánh log console của 2 lần chạy để thấy rõ sự khác biệt — đây chính là bằng chứng minh họa cho phần **Bằng chứng: Bản ghi màn hình** của đồ án.

---

## 💱 Bảng tỷ giá quy đổi (mặc định, có thể chỉnh trong `ExchangeRate`)

| Từ | Sang | Tỷ giá |
|---|---|---|
| USD | VND | 26,000 |
| EUR | VND | 30,000 |
| USD | EUR | 0.9 |
| EUR | USD | 1.1 |

## 💳 Hạn mức thấu chi mặc định (Checking)

| Loại tiền | Hạn mức thấu chi |
|---|---|
| VND | 5,000,000 |
| USD | 200 |
| EUR | 200 |

---

## 📄 Định dạng file dữ liệu

**`data.txt`** (được sinh bởi chức năng Save):
```
<Loại> <ID> <Số dư> <Mã tiền tệ (0=VND,1=USD,2=EUR)>
Savings 1 1000000 0
Checking 2 500 1
```

**`log.txt`** (lịch sử giao dịch, sinh bởi chức năng Export History):
```
ID <id>: [DEPOSIT/WITHDRAW/thời gian] <chi tiết>
```

---

## 🚀 Hướng phát triển thêm

- Thêm xác thực đăng nhập (username/password)
- Lưu dữ liệu dạng JSON/CSV thay vì text thuần
- Viết unit test cho `ExchangeRate` và `Transaction::transfer`
- Thêm giao diện đồ họa (Qt) hoặc REST API

## 👥 Thành viên nhóm

| Họ tên | Vai trò | MSSV |
|---|---|---|
| _(điền tên)_ | _(vd: Thiết kế lớp Account, Transaction)_ | |
| _(điền tên)_ | _(vd: Xử lý đa luồng, Demo mutex)_ | |
| _(điền tên)_ | _(vd: File I/O, Báo cáo)_ | |

## 📜 License

Đồ án phục vụ mục đích học tập.
