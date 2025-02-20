// Copyright (c) 2020 The DeFi Foundation
// Distributed under the MIT software license, see the accompanying
// file LICENSE or http://www.opensource.org/licenses/mit-license.php.

#ifndef DEFI_MASTERNODES_GOVVARIABLES_ATTRIBUTES_H
#define DEFI_MASTERNODES_GOVVARIABLES_ATTRIBUTES_H

#include <amount.h>
#include <masternodes/balances.h>
#include <masternodes/gv.h>
#include <masternodes/oracles.h>

enum VersionTypes : uint8_t {
    v0 = 0,
};

enum AttributeTypes : uint8_t {
    Live      = 'l',
    Oracles   = 'o',
    Param     = 'a',
    Token     = 't',
    Poolpairs = 'p',
    Locks     = 'L',
};

enum ParamIDs : uint8_t  {
    DFIP2201  = 'a',
    DFIP2203  = 'b',
    TokenID   = 'c',
    Economy   = 'e',
};

enum OracleIDs : uint8_t  {
    Splits    = 'a',
};

enum EconomyKeys : uint8_t {
    PaybackDFITokens = 'a',
    PaybackTokens    = 'b',
    DFIP2203Current  = 'c',
    DFIP2203Burned   = 'd',
    DFIP2203Minted   = 'e',
};

enum DFIPKeys : uint8_t  {
    Active       = 'a',
    Premium      = 'b',
    MinSwap      = 'c',
    RewardPct    = 'd',
    BlockPeriod  = 'e',
};

enum TokenKeys : uint8_t  {
    PaybackDFI            = 'a',
    PaybackDFIFeePCT      = 'b',
    LoanPayback           = 'c',
    LoanPaybackFeePCT     = 'd',
    DexInFeePct           = 'e',
    DexOutFeePct          = 'f',
    DFIP2203Enabled       = 'g',
    FixedIntervalPriceId  = 'h',
    LoanCollateralEnabled = 'i',
    LoanCollateralFactor  = 'j',
    LoanMintingEnabled    = 'k',
    LoanMintingInterest   = 'l',
    Ascendant             = 'm',
    Descendant            = 'n',
    Epitaph               = 'o',
};

enum PoolKeys : uint8_t {
    TokenAFeePCT = 'a',
    TokenBFeePCT = 'b',
};

struct CDataStructureV0 {
    uint8_t type;
    uint32_t typeId;
    uint32_t key;
    uint32_t keyId;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(type);
        READWRITE(typeId);
        READWRITE(VARINT(key));
        if (IsExtendedSize()) {
            READWRITE(keyId);
        } else {
            keyId = 0;
        }
    }

    bool IsExtendedSize() const {
        return type == AttributeTypes::Token
            && (key == TokenKeys::LoanPayback
            ||  key == TokenKeys::LoanPaybackFeePCT);
    }

    bool operator<(const CDataStructureV0& o) const {
        return std::tie(type, typeId, key, keyId) < std::tie(o.type, o.typeId, o.key, o.keyId);
    }
};

// for future use
struct CDataStructureV1 {
    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {}

     bool operator<(const CDataStructureV1& o) const { return false; }
};

struct CTokenPayback {
    CBalances tokensFee;
    CBalances tokensPayback;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(tokensFee);
        READWRITE(tokensPayback);
    }
};

ResVal<CScript> GetFutureSwapContractAddress();

using OracleSplits = std::map<uint32_t, int32_t>;
using DescendantValue = std::pair<uint32_t, int32_t>;
using AscendantValue = std::pair<uint32_t, std::string>;
using CAttributeType = boost::variant<CDataStructureV0, CDataStructureV1>;
using CAttributeValue = boost::variant<bool, CAmount, CBalances, CTokenPayback, CTokenCurrencyPair, OracleSplits, DescendantValue, AscendantValue>;

enum GovVarsFilter {
    All,
    NoAttributes,
    AttributesOnly,
    PrefixedAttributes,
    LiveAttributes,
    Version2Dot7,
};

class ATTRIBUTES : public GovVariable, public AutoRegistrator<GovVariable, ATTRIBUTES>
{
public:
    virtual ~ATTRIBUTES() override {}

    std::string GetName() const override {
        return TypeName();
    }

    Res Import(UniValue const &val) override;
    UniValue Export() const override;
    UniValue ExportFiltered(GovVarsFilter filter, const std::string &prefix) const;
    
    Res Validate(CCustomCSView const& mnview) const override;
    Res Apply(CCustomCSView &mnview, const uint32_t height) override;

    static constexpr char const * TypeName() { return "ATTRIBUTES"; }
    static GovVariable * Create() { return new ATTRIBUTES(); }

    template<typename T>
    [[nodiscard]] T GetValue(const CAttributeType& key, T value) const {
        auto it = attributes.find(key);
        if (it != attributes.end()) {
            if (auto val = boost::get<const T>(&it->second)) {
                value = std::move(*val);
            }
        }
        return std::move(value);
    }

    template<typename K, typename T>
    void SetValue(const K& key, T&& value) {
        static_assert(std::is_convertible_v<K, CAttributeType>);
        static_assert(std::is_convertible_v<T, CAttributeValue>);
        changed.insert(key);
        attributes[key] = std::forward<T>(value);
    }

    template<typename K>
    void EraseKey(const K& key) {
        static_assert(std::is_convertible_v<K, CAttributeType>);
        changed.insert(key);
        attributes.erase(key);
    }

    template<typename K>
    [[nodiscard]] bool CheckKey(const K& key) const {
        static_assert(std::is_convertible_v<K, CAttributeType>);
        return attributes.count(key) > 0;
    }

    template<typename C, typename K>
    void ForEach(const C& callback, const K& key) const {
        static_assert(std::is_convertible_v<K, CAttributeType>);
        static_assert(std::is_invocable_r_v<bool, C, K, CAttributeValue>);
        for (auto it = attributes.lower_bound(key); it != attributes.end(); ++it) {
            if (auto attrV0 = boost::get<K>(&it->first)) {
                if (!std::invoke(callback, *attrV0, it->second)) {
                    break;
                }
            }
        }
    }

    [[nodiscard]] const std::map<CAttributeType, CAttributeValue>& GetAttributesMap() const {
        return attributes;
    }

    ADD_OVERRIDE_VECTOR_SERIALIZE_METHODS
    ADD_OVERRIDE_SERIALIZE_METHODS(CDataStream)

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(attributes);
    }

    uint32_t time{0};

    // For formatting in export
    static const std::map<uint8_t, std::string>& displayVersions();
    static const std::map<uint8_t, std::string>& displayTypes();
    static const std::map<uint8_t, std::string>& displayParamsIDs();
    static const std::map<uint8_t, std::string>& displayOracleIDs();
    static const std::map<uint8_t, std::map<uint8_t, std::string>>& displayKeys();

    Res RefundFuturesContracts(CCustomCSView &mnview, const uint32_t height, const uint32_t tokenID = std::numeric_limits<uint32_t>::max());

private:
    friend class CGovView;
    bool futureBlockUpdated{};
    std::set<uint32_t> tokenSplits{};
    std::set<CAttributeType> changed;
    std::map<CAttributeType, CAttributeValue> attributes;

    // Defined allowed arguments
    static const std::map<std::string, uint8_t>& allowedVersions();
    static const std::map<std::string, uint8_t>& allowedTypes();
    static const std::map<std::string, uint8_t>& allowedParamIDs();
    static const std::map<std::string, uint8_t>& allowedLocksIDs();
    static const std::map<std::string, uint8_t>& allowedOracleIDs();
    static const std::map<uint8_t, std::map<std::string, uint8_t>>& allowedKeys();
    static const std::map<uint8_t, std::map<uint8_t,
            std::function<ResVal<CAttributeValue>(const std::string&)>>>& parseValue();

    Res ProcessVariable(const std::string& key, const std::string& value,
                        std::function<Res(const CAttributeType&, const CAttributeValue&)> applyVariable);
};

#endif // DEFI_MASTERNODES_GOVVARIABLES_ATTRIBUTES_H
