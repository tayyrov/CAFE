#include <vector>
#include <sstream>
#include <iterator>
#include <algorithm>

#include "lambdamu.h"
#include "Globals.h"
#include "cafe_commands.h"
#include "lambda.h"

using namespace std;

extern "C"
{
	extern pCafeParam cafe_param;
	void cafe_shell_set_lambda_mu(pCafeParam param, double* parameters);
	int __cafe_cmd_lambda_tree(pArgument parg);

#include "cafe.h"
}

void lambdamu_args::load(std::vector<Argument> args)
{
	lambda_arg_base::load(args);

	for (size_t i = 0; i < args.size(); i++)
	{
		pArgument parg = &args[i];

		if (!strcmp(parg->opt, "-t"))
		{
			cafe_param->num_mus = cafe_param->num_lambdas;
			mus.resize(cafe_param->num_mus);
		}
		else if (!strcmp(parg->opt, "-m"))
		{
			get_doubles_array(mus, parg);
			num_params += mus.size();
		}
		else if (!strcmp(parg->opt, "-eqbg"))
		{
			eqbg = 1;
		}
	}
}

int lambdamu_args::get_num_params() const
{
	if (lambda_tree == NULL)
	{
		if (k_weights.size() > 0) {
			return (lambdas.size()*(k_weights.size() - fixcluster0)) +
				(mus.size()*(k_weights.size() - fixcluster0)) +
				(k_weights.size() - 1);
		}
		else
			return lambdas.size() + (mus.size() - eqbg);
	}
	else
	{
		if (k_weights.size() > 0) {

			return (lambdas.size()*(k_weights.size() - fixcluster0)) +
				((mus.size() - eqbg)*(k_weights.size() - fixcluster0)) +
				(k_weights.size() - 1);
		}
		else
		{
			return lambdas.size() + mus.size();
		}
	}
}


void lambdamu_prepare_search(pCafeParam param, lambdamu_args& params)
{
	// prepare parameters
	if (params.lambda_tree != NULL) {
		// param->num_lambdas determined by lambda tree.
		param->eqbg = params.eqbg;
	}
	else {
		param->num_lambdas = 1;
		params.lambdas.resize(1);
		param->num_mus = 1;
		params.mus.resize(1);
		if (params.eqbg) {
			throw runtime_error("ERROR(lambdamu): Cannot use option eqbg without specifying a lambda tree. \n");
		}
	}

	if (params.k_weights.size() > 0) {
		param->parameterized_k_value = params.k_weights.size();
		param->fixcluster0 = params.fixcluster0;
	}

	param->num_params = params.get_num_params();
	initialize_params_and_k_weights(param, INIT_PARAMS | (params.k_weights.empty() ? 0 : INIT_KWEIGHTS));
	if (params.checkconv) { param->checkconv = 1; }
}

void lambda_arg_base::validate_parameter_count(int expected)
{
	ostringstream ost;
	// check if the numbers of lambdas and proportions put in matches the number of parameters
	if (expected != num_params)
	{
		ost << "ERROR (" << command() << "): The total number of parameters was not correct.\n";
		ost << "The total number of " << args();
		if (k_weights.size() > 0)
			ost << " and proportions (-p)";
		ost << " are " << num_params << " but " << expected << " were expected\n";
		ost << "based on the ";
		if (lambda_tree)
		{
			pString pstr = phylogeny_string(lambda_tree, NULL);
			ost << "tree " << pstr->buf;
			string_free(pstr);
		}
		if (k_weights.size() > 0)
		{
			if (lambda_tree)
				ost << " and ";
			ost << "the " << k_weights.size() << " clusters (-k).";
		}
		ost << "\n";
		throw runtime_error(ost.str());
	}
}

void lambdamu_set(pCafeParam param, lambdamu_args& params)
{
	if (params.lambda_tree != NULL) {
		// param->num_lambdas determined by lambda tree.
		param->eqbg = params.eqbg;
		if (!params.k_weights.empty()) {	// search clustered branch specific
			param->parameterized_k_value = params.k_weights.size();
			param->fixcluster0 = params.fixcluster0;
			param->num_params = params.get_num_params();

			params.validate_parameter_count(param->num_params);

			// copy user input into parameters
			initialize_params_and_k_weights(param, INIT_PARAMS | INIT_KWEIGHTS);

			// Not sure what values of fixcluster0 might cause this to NOT overrun
			size_t num_lambdas = params.lambdas.size()*(params.k_weights.size() - params.fixcluster0);
			size_t num_mus = ((params.mus.size() - params.eqbg)*(params.k_weights.size() - params.fixcluster0));

			input_values_set_lambdas(&param->input, &params.lambdas[0], min(num_lambdas, params.lambdas.size()));

			int first_mu = param->num_lambdas*(params.k_weights.size() - params.fixcluster0);
			input_values_set_mus(&param->input, &params.mus[0], first_mu, min(num_mus, params.mus.size()));

			int first_k_weight = (param->num_lambdas*(params.k_weights.size() - params.fixcluster0)) +
				((params.mus.size() - params.eqbg)*(params.k_weights.size() - params.fixcluster0));

			input_values_set_mus(&param->input, &params.k_weights[0], first_k_weight, min(num_mus, params.k_weights.size()-1));
		}
		else {	// search whole dataset branch specific
			param->num_params = params.lambdas.size() + (params.mus.size() - params.eqbg);

			params.validate_parameter_count(param->num_params);

			// copy user input into parameters
			initialize_params_and_k_weights(param, INIT_PARAMS);

			input_values_set_lambdas(&param->input, &params.lambdas[0], params.lambdas.size());
			input_values_set_mus(&param->input, &params.mus[0], param->num_lambdas, params.mus.size() - params.eqbg);
		}
	}
	else {
		param->num_lambdas = 1;
		params.lambdas.resize(1);
		param->num_mus = 1;
		params.mus.resize(1);
		if (params.eqbg) {
			throw runtime_error("ERROR(lambdamu): Cannot use option eqbg without specifying a lambda tree. \n");
		}
		if (params.k_weights.size() > 0) {				// search clustered whole tree
			param->parameterized_k_value = params.k_weights.size();
			param->fixcluster0 = params.fixcluster0;
			param->num_params = params.get_num_params();

			params.validate_parameter_count(param->num_params);

			// copy user input into parameters
			initialize_params_and_k_weights(param, INIT_PARAMS | INIT_KWEIGHTS);

			size_t num_lambdas = params.lambdas.size()*(params.k_weights.size() - params.fixcluster0);
			copy(params.lambdas.begin(), params.lambdas.begin() + min(params.lambdas.size(), num_lambdas), param->input.parameters);

			int first_mu = param->num_lambdas*(params.k_weights.size() - params.fixcluster0);
			size_t num_mus = params.mus.size()*(params.k_weights.size() - params.fixcluster0);
			copy(params.mus.begin(), params.mus.begin() + min(params.mus.size(), num_mus), param->input.parameters + first_mu);

			int first_k_weight = param->num_lambdas*(params.k_weights.size() - params.fixcluster0) + params.mus.size()*(param->parameterized_k_value - params.fixcluster0);
			copy(params.k_weights.begin(), params.k_weights.end() - 1, param->input.parameters + first_k_weight);
		}
		else {	// search whole dataset whole tree
			param->num_params = params.lambdas.size() + params.mus.size();

			// check if the numbers of lambdas and proportions put in matches the number of parameters
			params.validate_parameter_count(param->num_params);

			// copy user input into parameters
			initialize_params_and_k_weights(param, INIT_PARAMS);

			copy(params.lambdas.begin(), params.lambdas.end(), param->input.parameters);
			copy(params.mus.begin(), params.mus.end(), param->input.parameters + params.lambdas.size());
		}
	}
}

int cafe_cmd_lambdamu(Globals& globals, std::vector<std::string> tokens)
{
	pCafeParam param = &globals.param;

	prereqs(param, REQUIRES_FAMILY | REQUIRES_TREE);

	vector<Argument> args = build_argument_list(tokens);
	globals.Prepare();

	param->param_set_func = cafe_shell_set_lambda_mu;

	lambdamu_args params;
	params.load(args);

	// copy parameters collected to param based on the combination of options.
	param->posterior = 1;
	// set rootsize prior based on leaf size
	cafe_set_prior_rfsize_empirical(param);

	// search or set
	if (params.search) {
		lambdamu_prepare_search(param, params);
		// search
		cafe_best_lambda_mu_by_fminsearch(param, param->num_lambdas, param->num_mus, param->parameterized_k_value);

	}
	else {
		lambdamu_set(param, params);
		cafe_shell_set_lambda_mu(param, param->input.parameters);
	}

	if (param->pfamily)
	{
		reset_birthdeath_cache(param->pcafe, param->parameterized_k_value, &param->family_size);
	}

	cafe_log(param, "DONE: Lamda,Mu Search or setting, for command:\n");
	ostringstream ost;
	copy(tokens.begin(), tokens.end(), std::ostream_iterator<string>(ost, " "));
	cafe_log(param, "%s\n", ost.str().c_str());

	if (params.search && (param->parameterized_k_value > 0)) {
		// print the cluster memberships
		cafe_family_print_cluster_membership(param);
	}
	return 0;
}
