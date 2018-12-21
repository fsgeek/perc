#!/bin/bash

for i in  {1..9} 
do
    python tb.py perc-test-intelsdp1044-2018_11_29__14_50_04-emulated-pmem0.json --subtest test_cache_behavior_$i  > perc-test-intelsdp1044-2018_11_29__14_50_04-emulated-pmem0-test$i.json
    python tb.py perc-test-intelsdp1044-2018_11_29__14_50_04-actual-pmem1.json --subtest test_cache_behavior_$i  > perc-test-intelsdp1044-2018_11_29__14_50_04-actual-pmem1-test$i.json
    echo $i
done

exit 0


