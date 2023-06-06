#!/bin/bash

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

# Create CSV files for each operation
post_times="post_times.csv"
put_times="put_times.csv"
get_times="get_times.csv"
delete_times="delete_times.csv"

# Remove old CSV files if they exist
rm -f "$post_times" "$put_times" "$get_times" "$delete_times"

for i in {1..500}; do
    # POST request
    post_data='{
        "m2m:cin": {
            "rn": "cin-'"$i"'",
            "cnf": "application/json",
            "con": "{\"temperature\":28,\"timestamp\":1517912099}",
            "lbl": [
                "temperature"
            ]
        }
    }'

    post_time=$(curl --location --request POST 'http://10.79.11.253:8001/onem2m/lightbulb/state' \
    --header 'X-M2M-Origin: admin:admin' \
    --header 'Content-Type: application/json' \
    --data-raw "$post_data" -w "%{time_total}" -o /dev/null)

    echo "$post_time" >> "$post_times"

    # GET request
    get_time=$(curl --location --request GET "http://10.79.11.253:8001/onem2m/lightbulb/state/CIN-$i" \
    --header 'X-M2M-Origin: admin:admin' \
    --header 'Content-Type: application/json' \
    --data-raw '' -w "%{time_total}" -o /dev/null)

    echo "$get_time" >> "$get_times"

    # PUT request
    put_data='{
        "m2m:cnt": {
            "mni": 501,
            "lbl": [
                "temperature",
                "xpto2"
            ]
        }
    }'

    put_time=$(curl --location --request PUT 'http://10.79.11.253:8001/onem2m/lightbulb/state' \
    --header 'X-M2M-Origin: admin:admin' \
    --header 'Content-Type: application/json;ty=3' \
    --data-raw "$put_data" -w "%{time_total}" -o /dev/null)

    echo "$put_time" >> "$put_times"

    # DELETE request
    delete_time=$(curl --location --request DELETE "http://10.79.11.253:8001/onem2m/lightbulb/state/CIN-$i" \
    --header 'X-M2M-Origin: admin:admin' \
    --header 'Content-Type: application/json' \
    --data-raw '' -w "%{time_total}" -o /dev/null)

    echo "$delete_time" >> "$delete_times"
done

calculate_stats "POST" "$post_times"
calculate_stats "GET" "$get_times"
calculate_stats "PUT" "$put_times"
calculate_stats "DELETE" "$delete_times"
