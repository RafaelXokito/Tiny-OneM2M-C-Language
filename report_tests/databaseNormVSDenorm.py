import sqlite3
import time
import random
import string

def random_string(length):
    letters = string.ascii_lowercase
    return ''.join(random.choice(letters) for _ in range(length))

# connection setup
conn = sqlite3.connect(':memory:')
cur = conn.cursor()

# denormalized table
cur.execute('''
CREATE TABLE OneM2M_Denorm (
    ri TEXT PRIMARY KEY,
    pi TEXT
);
''')

# normalized tables
cur.execute('''
CREATE TABLE CSE_Base_Norm (
    ri TEXT PRIMARY KEY,
    pi TEXT
);
''')

cur.execute('''
CREATE TABLE AE_Norm (
    ri TEXT PRIMARY KEY,
    pi TEXT,
    FOREIGN KEY(pi) REFERENCES CSE_Base_Norm(ri)
);
''')

cur.execute('''
CREATE TABLE CNT_Norm (
    ri TEXT PRIMARY KEY,
    pi TEXT,
    FOREIGN KEY(pi) REFERENCES CSE_Base_Norm(ri)
);
''')

cur.execute('''
CREATE TABLE SUB_Norm (
    ri TEXT PRIMARY KEY,
    pi TEXT,
    FOREIGN KEY(pi) REFERENCES CSE_Base_Norm(ri)
);
''')

conn.commit()

# test size
test_size = 1000

# nested hierarchy depth
depth = 15

# denormalized table testing
start_time = time.time()
for _ in range(depth):
    for _ in range(test_size):
        cur.execute("INSERT INTO OneM2M_Denorm VALUES (?, ?)", (random_string(10), random_string(10)))
conn.commit()
print("Denormalized CREATE Average Time:", (time.time() - start_time)/(test_size*depth))

# Create a deep query on the denormalized table.
start_time = time.time()
for _ in range(test_size):
    cur.execute("SELECT * FROM OneM2M_Denorm WHERE pi IN (SELECT ri FROM OneM2M_Denorm WHERE pi IN (SELECT ri FROM OneM2M_Denorm WHERE pi IN (SELECT ri FROM OneM2M_Denorm)))")
print("Denormalized READ Average Time:", (time.time() - start_time)/test_size)

start_time = time.time()
for _ in range(test_size):
    cur.execute("UPDATE OneM2M_Denorm SET pi = ? WHERE ri = ?", (random_string(10), random_string(10)))
conn.commit()
print("Denormalized UPDATE Average Time:", (time.time() - start_time)/test_size)

start_time = time.time()
for _ in range(test_size):
    cur.execute("DELETE FROM OneM2M_Denorm WHERE ri = ?", (random_string(10),))
conn.commit()
print("Denormalized DELETE Average Time:", (time.time() - start_time)/test_size)

# normalized tables testing
for table_name in ["CSE_Base_Norm", "AE_Norm", "CNT_Norm", "SUB_Norm"]:
    start_time = time.time()
    for _ in range(depth):
        for _ in range(test_size):
            cur.execute(f"INSERT INTO {table_name} VALUES (?, ?)", (random_string(10), random_string(10)))
    conn.commit()
    print(f"Normalized {table_name} CREATE Average Time:", (time.time() - start_time)/(test_size*depth))

    # Create a deep query on the normalized tables, the complexity increases as we have to join tables
    start_time = time.time()
    for _ in range(test_size):
        cur.execute(f"""
            SELECT * FROM {table_name} t1 
            JOIN AE_Norm t2 ON t1.ri = t2.pi 
            JOIN CNT_Norm t3 ON t2.ri = t3.pi 
            JOIN SUB_Norm t4 ON t3.ri = t4.pi
        """)
    print(f"Normalized {table_name} READ Average Time:", (time.time() - start_time)/test_size)

    start_time = time.time()
    for _ in range(test_size):
        cur.execute(f"UPDATE {table_name} SET pi = ? WHERE ri = ?", (random_string(10), random_string(10)))
    conn.commit()
    print(f"Normalized {table_name} UPDATE Average Time:", (time.time() - start_time)/test_size)

    start_time = time.time()
    for _ in range(test_size):
        cur.execute(f"DELETE FROM {table_name} WHERE ri = ?", (random_string(10),))
    conn.commit()
    print(f"Normalized {table_name} DELETE Average Time:", (time.time() - start_time)/test_size)

cur.close()
