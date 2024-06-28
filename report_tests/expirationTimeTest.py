import sqlite3
import time
import random
import string

# Open a connection to an SQLite database file
conn = sqlite3.connect(':memory:')
cur = conn.cursor()

# Create tables
cur.execute('''
CREATE TABLE OneM2M_NoET (
    ri TEXT PRIMARY KEY,
    pi TEXT
);
''')

cur.execute('''
CREATE TABLE OneM2M_NonIndexedET (
    ri TEXT PRIMARY KEY,
    pi TEXT,
    et INTEGER
);
''')

cur.execute('''
CREATE TABLE OneM2M_IndexedET (
    ri TEXT PRIMARY KEY,
    pi TEXT,
    et INTEGER
);
''')

# Create an index on the et column
cur.execute('''
CREATE INDEX idx_et ON OneM2M_IndexedET (et);
''')

conn.commit()

def random_string(length):
    return ''.join(random.choices(string.ascii_letters + string.digits, k=length))

test_size = 10000

# Perform the CRUD operations on the three tables
for table_name in ["OneM2M_NoET", "OneM2M_NonIndexedET", "OneM2M_IndexedET"]:
    start_time = time.time()
    for i in range(test_size):
        if "IndexedET" in table_name:
            cur.execute(f"INSERT INTO {table_name} VALUES (?, ?, ?)", (i, random_string(10), i))
        else:
            cur.execute(f"INSERT INTO {table_name} VALUES (?, ?)", (i, random_string(10)))
    conn.commit()
    print(f"{table_name} CREATE Average Time:", (time.time() - start_time)/test_size)

    # READ operation
    start_time = time.time()
    for i in range(test_size):
        if "IndexedET" in table_name:
            cur.execute(f"SELECT * FROM {table_name} WHERE et < ?", (i,))
        else:
            cur.execute(f"SELECT * FROM {table_name}")
    print(f"{table_name} READ Average Time:", (time.time() - start_time)/test_size)

    # UPDATE operation
    start_time = time.time()
    for i in range(test_size):
        if "IndexedET" in table_name:
            cur.execute(f"UPDATE {table_name} SET et = ? WHERE et < ?", (i, i+1))
        else:
            cur.execute(f"UPDATE {table_name} SET pi = ? WHERE ri = ?", (random_string(10), i))
    conn.commit()
    print(f"{table_name} UPDATE Average Time:", (time.time() - start_time)/test_size)

    # DELETE operation
    start_time = time.time()
    for _ in range(test_size):
        if "IndexedET" in table_name:
            cur.execute(f"DELETE FROM {table_name} WHERE et < ?", (i+1,))
        else:
            cur.execute(f"DELETE FROM {table_name} WHERE ri = ?", (i,))
    conn.commit()
    print(f"{table_name} DELETE Average Time:", (time.time() - start_time)/test_size)

cur.close()
conn.close()
