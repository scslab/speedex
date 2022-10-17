#!/bin/bash

set +x

ansible-playbook -i awsinv ansible/synthetic_data_gen.yaml -e "exp_options=synthetic_data_config/synthetic_data_params_giant_50_more_cancels.yaml exp_name=giant50"

ansible-playbook -i awsinv ansible/set_experiment_options.yaml  -e "speedex_options=experiment_config/default_params_50.yaml data_folder=experiment_data/giant50/ results_folder=experiment_results/giant50/"
