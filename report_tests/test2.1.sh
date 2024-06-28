#!/bin/bash

# Define the number of iterations and concurrent processes
num_iterations=1000
num_concurrent_processes=10

# Functions for CRUD operations
perform_post() {
    count=$1
    time=$(curl -s -X POST "http://127.0.0.1:8001/onem2m" -H "Content-Type: application/vnd.onem2m-res+json" -d "{\"m2m:ae\": {\"api\": \"placeholder2\",\"rr\": \"true\",\"rn\": \"AE-${count}\",\"et\": \"20230428T234737\",\"lbl\": [\"interropetores\",\"xpto2\"],\"poa\": [\"http://127.0.0.1:1234\"],\"acpi\": [\"/id-in/acpCreateACPs\"]}}" -w "%{time_total}" -o /dev/null)
    echo "${time}"
}
perform_put() {
    count=$1
    time=$(curl -s -X PUT "http://127.0.0.1:8001/onem2m/AE-${count}" -H "Content-Type: application/vnd.onem2m-res+json" -d '{"m2m:ae": {"et": "20230531T234737","rr": "true","poa": ["http://127.0.0.1:4314"]}}' -w "%{time_total}" -o /dev/null)
    echo "${time}"
}
perform_get() {
    count=$1
    time=$(curl -s -X GET "http://127.0.0.1:8001/onem2m/AE-${count}" -w "%{time_total}" -o /dev/null)
    echo "${time}"
}
perform_delete() {
    count=$1
    time=$(curl -s -X DELETE "http://127.0.0.1:8001/onem2m/AE-${count}" -w "%{time_total}" -o /dev/null)
    echo "${time}"
}


export -f perform_post
export -f perform_put
export -f perform_get
export -f perform_delete

post_times="post_times.csv"
put_times="put_times.csv"
get_times="get_times.csv"
delete_times="delete_times.csv"

rm -f "$post_times"
rm -f "$put_times"
rm -f "$get_times"
rm -f "$delete_times"

seq 0 $((num_iterations - 1)) | xargs -I{} -P $num_concurrent_processes bash -c "perform_post {}" | awk '{print $0}' | tee -a "$post_times"
seq 0 $((num_iterations - 1)) | xargs -I{} -P $num_concurrent_processes bash -c "perform_put {}" | awk '{print $0}' | tee -a "$put_times"
seq 0 $((num_iterations - 1)) | xargs -I{} -P $num_concurrent_processes bash -c "perform_get {}" | awk '{print $0}' | tee -a "$get_times"
seq 0 $((num_iterations - 1)) | xargs -I{} -P $num_concurrent_processes bash -c "perform_delete {}" | awk '{print $0}' | tee -a "$delete_times"

# Helper function to calculate min, max, avg, and std deviation
calculate_stats() {
    local operation=$1
    local file=$2

    echo "Calculating stats for ${operation}"
    awk -F, '{
        sum+=$1; sumsq+=$1*$1; if (NR==1) {min=max=$1} else {if ($1>max) max=$1; if ($1<min) min=$1}
    } END {
        printf("Min: %.6f\nMax: %.6f\nAvg: %.6f\nStd Dev: %.6f\n", min, max, sum/NR, sqrt(sumsq/NR - (sum/NR)^2))
    }' "$file"
}

# Calculate and display stats for each operation
calculate_stats "POST" "$post_times"
calculate_stats "PUT" "$put_times"
calculate_stats "GET" "$get_times"
calculate_stats "DELETE" "$delete_times"
