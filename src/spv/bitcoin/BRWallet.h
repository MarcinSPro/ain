//
//  BRWallet.h
//
//  Created by Aaron Voisine on 9/1/15.
//  Copyright (c) 2015 breadwallet LLC
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.

#ifndef BRWallet_h
#define BRWallet_h

#include "BRTransaction.h"
#include "BRAddress.h"
#include "BRBIP32Sequence.h"
#include "BRInt.h"

#include <string>
#include <set>
#include <vector>

// Convert SPV UInt256 to Bitcoin uint256
uint256 to_uint256(UInt256 const & i);

#ifdef __cplusplus
extern "C" {
#endif

#define DEFAULT_FEE_PER_KB (TX_FEE_PER_KB*10)                  // 10 satoshis-per-byte
#define MIN_FEE_PER_KB     TX_FEE_PER_KB                       // defid 0.12 default min-relay fee
#define MAX_FEE_PER_KB     ((TX_FEE_PER_KB*1000100 + 190)/191) // slightly higher than a 10,000bit fee on a 191byte tx

typedef struct {
    UInt256 hash;
    uint32_t n;
} BRUTXO;

inline static size_t BRUTXOHash(const void *utxo)
{
    // (hash xor n)*FNV_PRIME
    return (size_t)((((const BRUTXO *)utxo)->hash.u32[0] ^ ((const BRUTXO *)utxo)->n)*0x01000193);
}

inline static int BRUTXOEq(const void *utxo, const void *otherUtxo)
{
    return (utxo == otherUtxo || (UInt256Eq(((const BRUTXO *)utxo)->hash, ((const BRUTXO *)otherUtxo)->hash) &&
                                  ((const BRUTXO *)utxo)->n == ((const BRUTXO *)otherUtxo)->n));
}

// Wallet struct
typedef struct BRWalletStruct BRWallet;

// Type to hold user addresses added from the DeFi wallet store
typedef std::set<UInt160, UInt160Compare> BRUserAddresses;

// allocates and populates a BRWallet struct that must be freed by calling BRWalletFree()
// forkId is 0 for bitcoin, 0x40 for b-cash
BRWallet *BRWalletNew(BRTransaction *transactions[], size_t txCount, BRMasterPubKey mpk, int forkId,
                      BRUserAddresses *userAddresses, BRUserAddresses *htlcAddresses);

// not thread-safe, set callbacks once after BRWalletNew(), before calling other BRWallet functions
// info is a void pointer that will be passed along with each callback call
// void balanceChanged(void *, uint64_t) - called when the wallet balance changes
// void txAdded(void *, BRTransaction *) - called when transaction is added to the wallet
// void txUpdated(void *, const UInt256[], size_t, uint32_t, uint32_t, const UInt256&)
//   - called when the blockHeight or timestamp of previously added transactions are updated
// void txDeleted(void *, UInt256, int, int) - called when a previously added transaction is removed from the wallet
void BRWalletSetCallbacks(BRWallet *wallet, void *info,
                          void (*balanceChanged)(void *info, uint64_t balance),
                          void (*txAdded)(void *info, BRTransaction *tx),
                          void (*txUpdated)(void *info, const UInt256 txHashes[], size_t txCount, uint32_t blockHeight,
                                            uint32_t timestamp, const UInt256& blockHash),
                          void (*txDeleted)(void *info, UInt256 txHash, int notifyUser, int recommendRescan));

// wallets are composed of chains of addresses
// each chain is traversed until a gap of a number of addresses is found that haven't been used in any transactions
// this function writes to addrs an array of <gapLimit> unused addresses following the last used address in the chain
// the internal chain is used for change addresses and the external chain for receive addresses
// addrs may be NULL to only generate addresses for BRWalletContainsAddress()
// returns the number addresses written to addrs
size_t BRWalletUnusedAddrs(BRWallet *wallet, BRAddress addrs[], uint32_t gapLimit, uint32_t internal);

// Import single uint160 into wallet
void BRWalletImportAddress(BRWallet *wallet, const uint160& userHash, const bool htlc = false);

// Add Bitcoin public key to SPV wallet from DeFi public key
bool BRWalletAddSingleAddress(BRWallet *wallet, const uint8_t &pubKey, const size_t pkLen, BRAddress& addr);

// Add previously created Bitcoin addresses from DeFi address book
void BRWalletAddUserAddresses(BRWallet *wallet);

// returns the first unused external address (bech32 pay-to-witness-pubkey-hash)
BRAddress BRWalletReceiveAddress(BRWallet *wallet);

// returns the first unused external address (legacy pay-to-pubkey-hash)
BRAddress BRWalletLegacyAddress(BRWallet *wallet);

// writes all addresses previously genereated with BRWalletUnusedAddrs() to addrs
// returns the number addresses written, or total number available if addrs is NULL
size_t BRWalletAllAddrs(BRWallet *wallet, BRAddress addrs[], size_t addrsCount);

// true if the address was previously generated by BRWalletUnusedAddrs() (even if it's now used)
int BRWalletContainsAddress(BRWallet *wallet, const char *addr);

// true if the address was previously used as an input or output in any wallet transaction
int BRWalletAddressIsUsed(BRWallet *wallet, const char *addr);

// writes transactions registered in the wallet, sorted by date, oldest first, to the given transactions array
// returns the number of transactions written, or total number available if transactions is NULL
size_t BRWalletTransactions(BRWallet *wallet, BRTransaction *transactions[], size_t txCount);

// writes transactions registered in the wallet, and that were unconfirmed before blockHeight, to the transactions array
// returns the number of transactions written, or total number available if transactions is NULL
size_t BRWalletTxUnconfirmedBefore(BRWallet *wallet, BRTransaction *transactions[], size_t txCount,
                                   uint32_t blockHeight);

// current wallet balance, not including transactions known to be invalid
uint64_t BRWalletBalance(BRWallet *wallet);

// total amount spent from the wallet (exluding change)
uint64_t BRWalletTotalSent(BRWallet *wallet);

// total amount received by the wallet (exluding change)
uint64_t BRWalletTotalReceived(BRWallet *wallet);

// writes unspent outputs to utxos and returns the number of outputs written, or number available if utxos is NULL
size_t BRWalletUTXOs(BRWallet *wallet, BRUTXO utxos[], size_t utxosCount);

// fee-per-kb of transaction size to use when creating a transaction
uint64_t BRWalletFeePerKb(BRWallet *wallet);
void BRWalletSetFeePerKb(BRWallet *wallet, uint64_t feePerKb);

// returns an unsigned transaction that sends the specified amount from the wallet to the given address
// result must be freed using BRTransactionFree()
BRTransaction *BRWalletCreateTransaction(BRWallet *wallet, uint64_t amount, const char *addr, std::string changeAddress, uint64_t feeRate);

// returns an unsigned transaction that satisifes the given transaction outputs
// result must be freed using BRTransactionFree()
BRTransaction *BRWalletCreateTxForOutputs(BRWallet *wallet, const BRTxOutput outputs[], size_t outCount, std::string changeAddress, uint64_t feeRate = 0);

// signs any inputs in tx that can be signed using private keys from the wallet
// seed is the master private key (wallet seed) corresponding to the master public key given when the wallet was created
// returns true if all inputs were signed, or false if there was an error or not all inputs were able to be signed
int BRWalletSignTransaction(BRWallet *wallet, BRTransaction *tx, const void *seed, size_t seedLen);

// true if the given transaction is associated with the wallet (even if it hasn't been registered)
int BRWalletContainsTransaction(BRWallet *wallet, const BRTransaction *tx);

// adds a transaction to the wallet, or returns false if it isn't associated with the wallet
int BRWalletRegisterTransaction(BRWallet *wallet, BRTransaction *tx);

// removes a tx from the wallet, along with any tx that depend on its outputs
void BRWalletRemoveTransaction(BRWallet *wallet, UInt256 txHash);

// returns the transaction with the given hash if it's been registered in the wallet
BRTransaction *BRWalletTransactionForHash(BRWallet *wallet, UInt256 txHash);

// true if no previous wallet transaction spends any of the given transaction's inputs, and no inputs are invalid
int BRWalletTransactionIsValid(BRWallet *wallet, const BRTransaction *tx);

// true if transaction cannot be immediately spent (i.e. if it or an input tx can be replaced-by-fee)
int BRWalletTransactionIsPending(BRWallet *wallet, const BRTransaction *tx);

// true if tx is considered 0-conf safe (valid and not pending, timestamp is greater than 0, and no unverified inputs)
int BRWalletTransactionIsVerified(BRWallet *wallet, const BRTransaction *tx);

// set the block heights and timestamps for the given transactions
// use height TX_UNCONFIRMED and timestamp 0 to indicate a tx should remain marked as unverified (not 0-conf safe)
void BRWalletUpdateTransactions(BRWallet *wallet, const UInt256 txHashes[], size_t txCount, uint32_t blockHeight,
                                uint32_t timestamp, const UInt256 &blockHash);
    
// marks all transactions confirmed after blockHeight as unconfirmed (useful for chain re-orgs)
void BRWalletSetTxUnconfirmedAfter(BRWallet *wallet, uint32_t blockHeight);

// returns the amount received by the wallet from the transaction (total outputs to change and/or receive addresses)
uint64_t BRWalletAmountReceivedFromTx(BRWallet *wallet, const BRTransaction *tx);

// returns the amount sent from the wallet by the trasaction (total wallet outputs consumed, change and fee included)
uint64_t BRWalletAmountSentByTx(BRWallet *wallet, const BRTransaction *tx);

// returns the fee for the given transaction if all its inputs are from wallet transactions, UINT64_MAX otherwise
uint64_t BRWalletFeeForTx(BRWallet *wallet, const BRTransaction *tx);

// historical wallet balance after the given transaction, or current balance if transaction is not registered in wallet
uint64_t BRWalletBalanceAfterTx(BRWallet *wallet, const BRTransaction *tx);

// fee that will be added for a transaction of the given size in bytes
uint64_t BRWalletFeeForTxSize(BRWallet *wallet, size_t size);

// fee that will be added for a transaction of the given amount
uint64_t BRWalletFeeForTxAmount(BRWallet *wallet, uint64_t amount);

// outputs below this amount are uneconomical due to fees (TX_MIN_OUTPUT_AMOUNT is the absolute minimum output amount)
uint64_t BRWalletMinOutputAmountWithFeePerKb(BRWallet *wallet, uint64_t feePerKb);

// maximum amount that can be sent from the wallet to a single address after fees
uint64_t BRWalletMaxOutputAmount(BRWallet *wallet);

// Check if transaction is spent
bool BRWalletTxSpent(BRWallet *wallet, const BRTransaction *tx, const uint32_t output, uint256& spent);

// frees memory allocated for wallet, and calls BRTransactionFree() for all registered transactions
void BRWalletFree(BRWallet *wallet);

// returns the given amount (in satoshis) in local currency units (i.e. pennies, pence)
// price is local currency units per bitcoin
int64_t BRLocalAmount(int64_t amount, double price);

// returns the given local currency amount in satoshis
// price is local currency units (i.e. pennies, pence) per bitcoin
int64_t BRBitcoinAmount(int64_t localAmount, double price);

#ifdef __cplusplus
}
#endif

// Get HTLC secret for contract address.
std::string BRGetHTLCSeed(BRWallet *wallet, const uint8_t *md20);

// Returns a set of all user related TXIDs.
std::set<std::string> BRListUserTransactions(BRWallet *wallet);

// Returns a vector of all HTLC relates transactions
std::vector<std::pair<BRTransaction *, size_t> > BRListHTLCReceived(BRWallet *wallet, const UInt160 &addr);

// Returns the raw hex encoded transaction data if found
std::string BRGetRawTransaction(BRWallet *wallet, UInt256 txHash);

#endif // BRWallet_h
