#pragma once

#include <string>
#include <memory>
#include <vector>
#include <chrono>

#include "simstring\simstring.h"

class RaceDateLibrary
{
public:
	struct Race {
		enum Grade {
			kG1 = 1 << 0,
			kG2 = 1 << 1,
			kG3 = 1 << 2,
			kOP = 1 << 3,
			kPreOP = 1 << 4,
		};
		Grade			grade;				// G1·G2·G3·OP·Pre-OP
		std::wstring	name;				// レース名
		enum GroundCondition {
			kGrass = 1 << 5,
			kDart  = 1 << 6,
		};
		GroundCondition	groundCondition;	// 芝·ダート
		enum DistanceClass {
			kSprint = 1 << 7,
			kMile	= 1 << 8,
			kMiddle	= 1 << 9,
			kLong	= 1 << 10,
		};
		DistanceClass	distanceClass;		// 短距離·マイル·中距離·長距離
		std::wstring	distance;			// 上の実際の距離数
		enum Rotation {
			kRight	= 1 << 11, 
			kLeft	= 1 << 12,
			kLine	= 1 << 13,
		};
		Rotation		rotation;			// 右·左回り·直線
		std::wstring	location;			// 場所
		enum Location {
			kSapporo	= 1 << 14,
			kHakodate	= 1 << 15,
			kHukusima	= 1 << 16,
			kNiigata	= 1 << 17,
			kTokyo		= 1 << 18,
			kNakayama	= 1 << 19,
			kTyuukyou	= 1 << 20,
			kKyoto		= 1 << 21,
			kHanshin	= 1 << 22,
			kOgura		= 1 << 23,
			kOoi		= 1 << 24,
			kMaxLocationCount = 11,
		};
		Location		locationFlag;
		std::vector<std::wstring>	date;	// 開催日

		// =============================
		std::wstring	RaceName() const;
		std::wstring	GroundConditionText() const;
		std::wstring	DistanceText() const;
		std::wstring	RotationText() const;

		// 同種類はOR検索、別種類はAND検索
		bool	IsMatchState(int32_t state);
	};

	// RaceDataLibrary を読み込む
	bool	LoadRaceDataLibrary();

	// 全ターンリスト
	const std::vector<std::wstring>& GetAllTurnList() const {
		return m_allTurnList;
	}
	// ターン順の全レースリスト
	const std::vector<std::vector<std::shared_ptr<Race>>>& GetTurnOrderedRaceList() const {
		return m_turnOrderedRaceList;
	}

	// 
	int		GetTurnNumberFromTurnName(const std::wstring& searchTurn);


	// あいまい検索で現在の日付を変更する
	std::wstring	AnbigiousChangeCurrentTurn(std::vector<std::wstring> ambiguousCurrentTurn);

private:
	void	_InitDB();

	std::wstring	m_currentTurn;
	int		m_searchCount = 0;	// 逆行·遡行防止のため

	std::vector<std::wstring> m_allTurnList;

	std::unique_ptr<simstring::reader>	m_dbReader;

	std::vector<std::vector<std::shared_ptr<Race>>>	m_turnOrderedRaceList;
};

