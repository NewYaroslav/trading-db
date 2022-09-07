#include <iostream>
#include "../../include/trading-db/parts/qdb-csv.hpp"
#include "../../include/trading-db/parts/qdb-writer-price-buffer.hpp"
#include "../../include/trading-db/parts/qdb-zstd.hpp"
#include "../../include/trading-db/parts/qdb-compact-dataset.hpp"

int main() {
    std::cout << "start train" << std::endl;
    const std::string path_traning_data = "..//..//dataset//training_data//candles//";
    const std::string file_name = "qdb-dictionary-candles.hpp";
    const std::string dict_name = "qdb_dictionary_candles";
    const bool is_res = trading_db::train_zstd(path_traning_data, file_name, dict_name, 102400);
    std::cout << "end train, result: " << is_res << std::endl;
    return 0;
}
