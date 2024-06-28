#!/bin/bash

DB_PATH="TinyOneM2M/tiny-oneM2M.db" # TODO: Replace this with the actual path to your SQLite database

for i in $(seq 1 50000); do
    sqlite3 "$DB_PATH" "INSERT INTO  \"mtc\" (\"ty\", \"ri\", \"rn\", \"pi\", \"et\", \"ct\", \"lt\", \"url\", \"lbl\", \"blob\", \"st\", \"cnf\", \"cs\", \"con\") VALUES ('4', 'CCIN${i}', 'cin-${i}', 'CCNT1', '2023-07-05 21:48:19', '2023-06-05 21:48:19', '2023-06-05 21:48:19', '/onem2m/lightbulb/state/cin-${i}', '["temperature"]', '{
    \"m2m:cin\":    {
        \"ct\":   \"20230605T214819\",
        \"ty\":   4,
        \"ri\":   \"CCIN${i}\",
        \"rn\":   \"cin-${i}\",
        \"pi\":   \"CCNT1\",
        \"aa\":   \"\",
        \"st\":   0,
        \"cnf\":  \"application/json\",
        \"cs\":   41,
        \"con\":  \"{\\\"temperature\\\":28,\\\"timestamp\\\":1517912099}\",
        \"et\":   \"20230705T214819\",
        \"or\":   \"\",
        \"lt\":   \"20230605T214819\",
        \"lbl\":  [\"temperature\"],
        \"at\":   []
    }
}', '0', 'application/json', '41', '{\"temperature\":28,\"timestamp\":1517912099}');"
done
