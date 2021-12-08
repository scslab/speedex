
from cryptocoin_experiment_xdr import *

import csv
import dateutil.parser as date_parser
import datetime


coin_folder = b'coingecko_snapshot/'
end_date = datetime.datetime(2021, 12, 8, 0, 0, 0, 0)
default_length = 500

# ls coingecko_snapshot/ | grep "\-usd" | grep -o "^[^-]\+" | sed "s/^[^-]*/\tb\"&\","/

coins = [
	b"AVAX",
	b"ada",
	b"algo",
	b"atom",
	b"axs",
	b"bat",
	b"bch",
	b"bnb",
	b"bsv",
	b"btc",
	b"btt",
	b"busd",
	b"chz",
	b"cro",
	b"crv",
	b"dai",
	b"dash",
	b"doge",
	b"dot",
	b"enj",
	b"eos",
	b"etc",
	b"eth",
	b"fil",
	b"ftm",
	b"gala",
	b"hot",
	b"icp",
	b"link",
	b"lrc",
	b"luna",
	b"mana",
	b"matic",
	b"mim",
	b"near",
	b"okb",
	b"sand",
	b"shib",
	b"sol",
	b"theta",
	b"trx",
	b"uni",
	b"usdc",
	b"usdt",
	b"vet",
	b"wbtc",
	b"xlm",
	b"xrp",
	b"xtz",
	b"zec"
]

def load_coin(coin_name, length):
	filename = coin_folder + coin_name +b'-usd-max.csv'
	output = {}

	with open(filename, 'r') as csvfile:
		reader = csv.reader(csvfile)
		header_row = True
		for row in reader:
			if header_row:
				print (row)
				header_row = False
				continue
			cur_date = date_parser.parse(row[0], ignoretz = True)
			delta = cur_date - end_date
			output[delta.days] = (float(row[1]), float(row[3]))

	x = Cryptocoin.new()

	for i in range(0, len(coin_name)):
		x.name.push_back(coin_name[i])

	discontinuity = False

	(price, volume) = (1, 0)

	for i in range(0, length):
		idx = i - length
		y = DateSnapshot.new()

		if not idx in output.keys():
			#print (idx, coin)
			#print (output.keys())
			if (discontinuity):
				print(idx, coin)
				print("using previous day's volume")
				#raise ValueError("not enough data in " + str(coin_name))
		else:
			(price, volume) = output[idx]
			discontinuity = True
		y.volume = volume
		y.price = price
		x.snapshots.push_back(y)
	return x

data = CryptocoinExperiment.new()

for coin in coins:
	data.coins.push_back(load_coin(coin, default_length))

data.save_to_file(coin_folder + b'unified_data')