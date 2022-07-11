#pragma once
#ifndef TRADING_DB_BET_DATABASE_PARTS_COMMON_HPP_INCLUDED
#define TRADING_DB_BET_DATABASE_PARTS_COMMON_HPP_INCLUDED

#include <string>

namespace trading_db {
	namespace bet_database {

		/// Направление ставки
		enum class ContractType {
			UNKNOWN_STATE = 0,
			BUY = 1,
			SELL = -1,
		};

		/// Тип ставки
		enum class BoType {
			SPRINT = 0,
			CLASSIC = 1,
		};

		/// Состояния сделки
		enum class BetStatus {
			UNKNOWN_STATE,			/**< Неопределенное состояние уже открытой сделки */
			OPENING_ERROR,			/**< Ошибка открытия */
			CHECK_ERROR,			/**< Ошибка проверки результата сделки */
			LOW_PAYMENT_ERROR,		/**< Низкий процент выплат */
			WAITING_COMPLETION,		/**< Ждем завершения сделки */
			WIN,					/**< Победа */
			LOSS,					/**< Убыток */
			STANDOFF,				/**< Ничья */
			UPDATE,					/**< Обновление состояния ставки */
			INCORRECT_PARAMETERS,	/**< Некорректные параметры ставки */
			AUTHORIZATION_ERROR		/**< Ошибкаавторизации */
		};

		enum StatsTypes {
			FIRST_BET,
			LAST_BET,
			ALL_BET,
		};

		/** \brief Класс ставки БО
		 */
		class BetData {
		public:
			int64_t uid			= 0;	/// ключ - уникальный ID сделки в БД
			int64_t broker_id	= 0;	/// уникальный номер сделки, который присваивает брокер
			int64_t open_date	= 0;	/// метка времени открытия сделки в миллисекундах
			int64_t close_date	= 0;	/// метка времени закрытия сделки в миллисекундах
			double open_price	= 0;	/// цена входа в сделку
			double close_price	= 0;	/// цена выхода из сделки

			double amount		= 0;	/// размер ставки
			double profit		= 0;	/// размер выплаты
			double payout		= 0;	/// процент выплат
			double winrate		= 0;	/// винрейт сигнала

			int64_t delay		= 0;	/// задержка на открытие ставки в миллисекундах
			int64_t ping		= 0;	/// пинг запроса на открытие ставки в миллисекундах

			uint32_t duration	= 0;	/// экспирация (длительность) бинарного опциона в секундах
			uint32_t step		= 0;	/// шаг систем риск менеджмента
			bool demo			= true;	/// флаг демо аккаунта
			bool last			= true;	/// флаг последней сделки - для подсчета винрейта в системах риск-менджента типа мартингейла и т.п.

			ContractType contract_type	= ContractType::UNKNOWN_STATE;	/// тип контракта, см.BetContractType
			BetStatus status			= BetStatus::UNKNOWN_STATE;		/// состояние сделки, см.BetStatus
			BoType type					= BoType::SPRINT;				/// тип бинарного опциона(SPRINT, CLASSIC и т.д.), см.BetType

			std::string symbol;		/// имя символа(валютная пара, акции, индекс и пр., например EURUSD)
			std::string broker;		/// имя брокера
			std::string currency;	/// валюта ставки
			std::string signal;		/// имя сигнала, стратегии или индикатора, короче имя источника сигнала
			std::string comment;	/// комментарий
			std::string user_data;	/// данные пользователя

			BetData() {};
		};
	};
};
#endif // TRADING_DB_BET_DATABASE_PARTS_COMMON_HPP_INCLUDED
