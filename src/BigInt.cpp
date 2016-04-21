#include <cassert>
#include <algorithm>
#include <math.h>
#include "../logger/logger.h"
#include "../include/BigInt.h"


#define WORD_BITS			32
#define BYTE_BITS			8
#define HEX_CHAR_BITS			4


/* macros for whole BigInt */
#define BIGINT_BITS			1024
#define BIGINT_BYTES			128	/* 1024 / 4 */
#define BIGINT_SIZE_IN_HEX		256

#define BIGINT_DOUBLE_BITS		2048

/*
 * for 1024 bit number need 35 blocks
 * 34 blocks with 30 bits digit
 * and 1 block with 4 bits
 */

/* macros for usual block of BigInt */
#define BLOCK_BITS			30
#define BLOCK_CARRY_BITS		2
#define BLOCK_MAX_NUMBER		0x3FFFFFFF


BigInt::BigInt(unsigned int lengthBits):
	length_(lengthBits),
	size_(ceil(lengthBits / (float)BLOCK_BITS)),
	countBistLastBlock_(lengthBits % BLOCK_BITS),
	maxValueLastBlock_(fillBits(countBistLastBlock_))
{
	assert(lengthBits == BIGINT_BITS || lengthBits == BIGINT_DOUBLE_BITS);

//	INFO("Create BigInt with length {}, size {}, countBistLastBlock {},"
//			"maxValueLastBlock_ {:X}",
//			length_, size_, countBistLastBlock_, maxValueLastBlock_);

	blocks_ = new block[size_];
	memset(blocks_, 0, sizeof(block) * size_);
	preComputedTable_ = NULL;
}

BigInt::BigInt() : BigInt(BIGINT_BITS)
{
}

BigInt::BigInt(const char *strHexNumber) : BigInt()
{
	if (fromString(strHexNumber)) {
		WARN("Can not convert from string. Check your string.");
	}
}

BigInt::BigInt(std::string &strHexNumber) : BigInt()
{
	if (fromString(strHexNumber)) {
		WARN("Can not convert from string. Check your string.");
	}
}

BigInt::BigInt(BigInt&& number) : length_(number.length_), size_(number.size_),
	countBistLastBlock_(number.countBistLastBlock_),
	maxValueLastBlock_(number.countBistLastBlock_)
{
	DEBUG("Move constructor called");

	blocks_ = number.blocks_;
	number.blocks_ = nullptr;
}

BigInt::~BigInt() { delete[] blocks_; }

BigInt* BigInt::getDoubleNumber()
{
	return new BigInt(BIGINT_DOUBLE_BITS);
}

unsigned int BigInt::getLength()
{
	return length_;
}

int BigInt::fromString(const char *hexStr)
{
	int i = 0;
	size_t maxAmountChars = length_ / HEX_CHAR_BITS;
	std::vector<block> rawArray(length_ / WORD_BITS);
	std::fill(rawArray.begin(), rawArray.end(), 0);

	if (strlen(hexStr) > maxAmountChars) {
		WARN("String too long! Length is '{}'. Possible length is '{}'",
				strlen(hexStr), maxAmountChars);
	}

	for (i = (int)strlen(hexStr) - 1; i >= 0; --i) {
		if (hexCharToInteger(hexStr[i]) == -1) {
			WARN("Provided bad nuber ({})", hexStr);
			return -1;
		}
	}
	unsigned int indexArray = 0;
	unsigned int shiftPos = 0;
	i = (int)strlen(hexStr) - 1;
	for (; i >= 0 && indexArray < rawArray.size(); --i) {
		int digit = hexCharToInteger(hexStr[i]);
		rawArray[indexArray] += digit << 4 * shiftPos;
		++shiftPos;
		if (shiftPos % 8 == 0) {
			++indexArray;
			shiftPos = 0;
		}
	}
	//DEBUG("Raw array");
	//for(i = 0; i < rawArray.size(); ++i) {
	//	DEBUG("{})\t{:x}", i, rawArray[i]);
	//}
	rawArrayToBlocks(rawArray);

	return 0;
}

int BigInt::fromString(const std::string &hexString)
{
	return fromString(hexString.c_str());
}

std::string BigInt::toString() const
{
	std::string output("");
	unsigned int i = 0;
	std::vector<block> rawArray(length_ / WORD_BITS);
	std::fill(rawArray.begin(), rawArray.end(), 0);

	blocksToRawArray(rawArray);

	//INFO("BigInt number (normal array): size is {}", rawArray.size());
	for(i = 0; i < rawArray.size(); ++i) {
		//INFO("{}\t{:X}", i, rawArray[i]);
		for (int j = 0; j < 8; ++j) {
			char ch = rawArray[i] >> (j * 4) & 0xF;
			output.push_back(integerToHexChar(ch));
			//DEBUG("parse {} as {}", (int)ch, integerToHexChar(ch));
		}
	}
	//INFO("End of BigInt ( normal array)");
	std::reverse(output.begin(), output.end());
	return output;
}

/**
 * @brief 			Convert char in hex format to number.
 * @param digit			- INPUT. Char to convert.
 * @return 			integer in range [0 - 15] when successful and
 * 				-1 if error occurred.
 */
int BigInt::hexCharToInteger(char digit)
{
	if (digit >= '0' && digit <= '9') {
		return digit - '0' ;
	} else if (digit >= 'a' && digit <= 'f') {
		return 10 + digit - 'a' ;
	} else if (digit >= 'A' && digit <= 'F') {
		return 10 + digit - 'A' ;
	}
	return -1;
}

char BigInt::integerToHexChar(int symbol) const
{
	assert(symbol >= 0x0 && symbol <= 0xF);
	if (symbol >= 0 && symbol <= 9) {
		return '0' + symbol;
	} else {
		return 'A' + symbol - 10;
	}
}

void BigInt::rawArrayToBlocks(std::vector<block> &rawArray)
{
	unsigned int leftBits = 0;
	block temp = 0;

	unsigned int indexRaw = 0;
	unsigned int indexBlocks = 0;

	for(; indexRaw < rawArray.size(); ++indexRaw, ++indexBlocks) {
		temp = 0;
		if (leftBits) {
			temp = rawArray[indexRaw - 1] >> (WORD_BITS - leftBits);
			if (leftBits == BLOCK_BITS) {
				blocks_[indexBlocks] = temp;
				--indexRaw;
				leftBits = 0;
				//DEBUG("30 bits| temp = {:X}", temp);
				continue;
			}
		}
		blocks_[indexBlocks] = (rawArray[indexRaw] <<
				(leftBits + BLOCK_CARRY_BITS));
		blocks_[indexBlocks] >>= BLOCK_CARRY_BITS;
		//DEBUG("T: {:X} and {:X}", blocks_[indexBlocks], temp);
		blocks_[indexBlocks] += temp;
		leftBits += BLOCK_CARRY_BITS;
	}

	assert(leftBits == countBistLastBlock_);
	assert(indexBlocks == size_ - 1);

	blocks_[indexBlocks] = rawArray[indexRaw - 1] >> (WORD_BITS - leftBits);
}

/**
 * @brief 			Convert numbers in block representation to
 * 				human readable format.
 * @param rawArray		[output] Array for numbers.
 */
void BigInt::blocksToRawArray(std::vector<block> &rawArray) const
{
	unsigned int acquiredBits = 0;
	int indexBlocks = 0;
	int indexRaw = 0;
	int sizeRawArray = rawArray.size();
	block temp = 0;

	for(; indexRaw < sizeRawArray; ++indexBlocks, ++indexRaw) {
		if(acquiredBits == BLOCK_BITS) {
			++indexBlocks;
			acquiredBits = 0;
		}
		rawArray[indexRaw] = blocks_[indexBlocks] >> acquiredBits;
		temp = blocks_[indexBlocks + 1] <<
			(WORD_BITS - acquiredBits - BLOCK_CARRY_BITS);
		//DEBUG("BtA: {:X} + \t {:X}   {}", rawArray[indexRaw], temp,
		//		(WORD_BITS - acquiredBits));
		rawArray[indexRaw] += temp;
		acquiredBits += BLOCK_CARRY_BITS;
	}
}

int BigInt::getPosMostSignificatnBit() const
{
	int i;
	int position = length_ - 1;

	if (blocks_[size_ - 1]) {
		assert(blocks_[size_ - 1] <= maxValueLastBlock_);
		return position - (__builtin_clz(blocks_[size_ - 1]) - (WORD_BITS - countBistLastBlock_));
	}
	position -= countBistLastBlock_;

	for (i = size_ - 2; i >= 0 && blocks_[i] == 0; --i) {
		position -= BLOCK_BITS;
	}
	if (i == -1) {
		// number is zero
		return -1;
	} else {
		assert(blocks_[i] <= BLOCK_MAX_NUMBER);
		return position - (__builtin_clz(blocks_[i]) - (WORD_BITS - BLOCK_BITS));
	}
}

int BigInt::isEqual(const BigInt &number)
{
	block *end, *start;

	if (size_ == number.size_) {
		return std::equal(blocks_, blocks_ + size_, number.blocks_);
	} else if (size_ > number.size_) {
		start = blocks_ + number.size_;
		end = blocks_ + size_;
		while (start != end) {
			if (*start++ != 0) {
				return false;
			}
		}
		return std::equal(blocks_, blocks_ + number.size_, number.blocks_);
	} else {
		start = number.blocks_ + size_;
		end = number.blocks_ + number.size_;
		while (start != end) {

			if (*start++ != 0) {
				return false;
			}
		}
		return std::equal(blocks_, blocks_ + size_, number.blocks_);
	}
}

bool BigInt::isZero() const
{
	//block toCmp[size_] = {0};
	//return !memcmp(blocks_, toCmp, size_ * sizeof(block));
	block *start = blocks_;
	block *end = blocks_ + size_;
	while (start != end) {
		if (*start) {
			return false;
		}
		++start;
	}
	return true;
}

void BigInt::setMax()
{
	std::fill(blocks_, blocks_ + size_ - 1, BLOCK_MAX_NUMBER);
	blocks_[size_ - 1] = maxValueLastBlock_;
}

void BigInt::setZero()
{
	memset(blocks_, 0, size_ * sizeof(block));
}

void BigInt::setNumber(unsigned int number)
{
	memset(blocks_ + 1, 0, (size_ - 1) * sizeof(block));
	blocks_[0] = number;
}

block BigInt::fillBits(unsigned int amountBits)
{
	block number = 0;

	for (unsigned int i = 0; i < amountBits; ++i) {
		number += (1 << i);
	}
	return number;
}

void BigInt::shiftLeftBlock(unsigned int countBits)
{
	assert(countBits <= BLOCK_BITS);

	block carryBits = 0;
	block temp = 0;
	unsigned int i = 0;

	for (i = 0; i < size_ - 1; ++i) {
		// acquire carry bits from current block
		temp = blocks_[i] >> (BLOCK_BITS - countBits);
		blocks_[i] = (blocks_[i] << countBits) & BLOCK_MAX_NUMBER;
		// add carry bits from previuos block
		blocks_[i] += carryBits;
		assert(blocks_[i] <= BLOCK_MAX_NUMBER);
		carryBits = temp;
	}
	// last block
	blocks_[i] = (blocks_[i] << countBits) + carryBits;
	blocks_[i] &= maxValueLastBlock_;
}

void BigInt::shiftLeft(unsigned int countBits)
{
	if (countBits >= length_) {
		setZero();
		return;
	}

	while (countBits > BLOCK_BITS) {
		shiftLeftBlock(BLOCK_BITS);
		countBits -= BLOCK_BITS;
	}
	shiftLeftBlock(countBits);
}

void BigInt::shiftRightBlock(unsigned int countBits)
{
	assert(countBits <= BLOCK_BITS);
	unsigned int i = 0;
	block maxCarryBlock = fillBits(countBits);
	block carryBits = 0;

	blocks_[0] >>= countBits;
	for (i = 1; i < size_; ++i) {
		carryBits = blocks_[i] & maxCarryBlock;
		blocks_[i - 1] += carryBits << (BLOCK_BITS - countBits);
		blocks_[i] >>= countBits;
	}
}

void BigInt::shiftRight(unsigned int countBits)
{
	if (countBits >= length_) {
		setZero();
		return;
	}
	while (countBits > BLOCK_BITS) {
		shiftRightBlock(BLOCK_BITS);
		countBits -= BLOCK_BITS;
	}
	shiftRightBlock(countBits);
}

///
///  1 if this > number
/// -1 if number > this
///  0 if this == number
///
int BigInt::cmp(const BigInt &number) const
{
	unsigned int i = 0;
	unsigned int size = size_;
	int diff = 0;
	int j = 0;

	if (size_ > number.size_) {
		for (i = size_ - 1; i > number.size_ - 1; --i) {
			if (blocks_[i]) {
				return 1;
			}
		}
		size = number.size_;
	} else if (number.size_ > size_) {
		for (i = number.size_ - 1; i > size_ - 1; --i) {
			if (number.blocks_[i]) {
				return -1;
			}
		}
		size = size_;
	}

	diff = blocks_[size - 1] - number.blocks_[size - 1];
	for (j = size - 2; j >= 0 && diff == 0; --j) {
		diff = blocks_[j] - number.blocks_[j];
	}
	return diff > 0 ? 1 : diff < 0 ? -1 : 0;
}

void BigInt::setBit(unsigned int position, unsigned int value) const
{
	assert((value & 1) == value);
	assert(position >= 0 && position < length_);

	int posInBlock = position % BLOCK_BITS;
	int posBlock = position / BLOCK_BITS;

	block temp = (~(1 << posInBlock)) & BLOCK_MAX_NUMBER;
	// reset needed bit
	blocks_[posBlock] &= temp;
	// set new value
	blocks_[posBlock] |= (block)value << posInBlock;
	assert(blocks_[size_ - 1] <= maxValueLastBlock_);
}

int BigInt::getBit(unsigned int position) const
{
	assert(position >= 0 && position <= length_);

	block neededBlock = blocks_[position / BLOCK_BITS];
	int posInBlock = position % BLOCK_BITS;
	return (neededBlock & ( 1 << posInBlock )) >> posInBlock;
}

int BigInt::clearBit(unsigned int position)
{
	block neededBlock = blocks_[position / BLOCK_BITS];
	int posInBlock = position % BLOCK_BITS;

	int bit = (neededBlock >> posInBlock) & 1;
	blocks_[position / BLOCK_BITS] &= ~(1 << posInBlock);
	return bit;
}

//BigInt &BigInt::copy() const
//{
//	BigInt number(length_);
//	unsigned int i;

//	for (i = 0; i < size_; ++i) {
//		number.blocks_[i] = blocks_[i];
//	}
//	return number;
//}

BigInt* BigInt::copy() const
{
	BigInt *number = new BigInt(length_);
	unsigned int i;

	for (i = 0; i < size_; ++i) {
		number->blocks_[i] = blocks_[i];
	}
	return number;
}

void BigInt::copyContent(const BigInt &number)
{
	block *thisData = blocks_;
	block *thisEnd = blocks_ + size_;

	block *mData = number.blocks_;
	block *mEnd = number.blocks_ + number.size_;


	while (thisData != thisEnd && mData != mEnd) {
		*thisData++ = *mData++;
	}
	blocks_[size_ - 1] &= maxValueLastBlock_;
}

bool BigInt::add(const BigInt &number)
{
	assert(size_ >= number.size_);

	block carry = 0;
	unsigned int i = 0;

	for(i = 0 ; i < number.size_; ++i) {
		blocks_[i] += number.blocks_[i] + carry;
		carry = blocks_[i] >> BLOCK_BITS;
		blocks_[i] &= BLOCK_MAX_NUMBER;
		assert((carry & 1) == carry);
	}

	if (size_ == number.size_) {
		carry = blocks_[number.size_ - 1] >> maxValueLastBlock_;
		blocks_[number.size_ - 1] &= maxValueLastBlock_;
	} else {
		for (i = number.size_; i < size_ - 1; ++i) {
			blocks_[i] += carry;
			carry = blocks_[i] >> BLOCK_BITS;
			blocks_[i] &= BLOCK_MAX_NUMBER;
			assert((carry & 1) == carry);
		}
		carry = blocks_[size_ - 1] >> maxValueLastBlock_;
		blocks_[size_ - 1] &= maxValueLastBlock_;
	}
	assert((carry & 1) == carry);
	return carry;
}

void BigInt::sub(const BigInt &number)
{
	assert(size_ >= number.size_);

	block setterCarryBit = BLOCK_MAX_NUMBER + (block)1;
	block carryBit = 0;
	unsigned int i = 0;

	assert(setterCarryBit == ((block)1 << BLOCK_BITS));

	for (i = 0; i < number.size_; ++i) {
		blocks_[i] |= setterCarryBit;
		blocks_[i] -= number.blocks_[i] + carryBit;
		carryBit = !(blocks_[i] >> BLOCK_BITS);
		blocks_[i] &= BLOCK_MAX_NUMBER;
		assert((carryBit & 1) == carryBit);
	}
	if (size_ == number.size_) {
		// for last block if nubers are equals
		blocks_[size_ - 1] &= maxValueLastBlock_;
	} else {
		for (i = number.size_;i < size_ - 1; ++i) {
			blocks_[i] |= setterCarryBit;
			blocks_[i] -= carryBit;
			carryBit = !(blocks_[i] >> BLOCK_BITS);
			blocks_[i] &= BLOCK_MAX_NUMBER;
			assert((carryBit & 1) == carryBit);
		}
		// last block bigger number
		blocks_[i] = blocks_[i] - carryBit;
		blocks_[i] &= maxValueLastBlock_;
	}
}

BigInt* BigInt::mul(const BigInt &number, BigInt **result)
{
	uint64_t temp_res = 0;
	block c = 0;
	BigInt *res = NULL;
	if (result) {
		if (*result) {
			res = *result;
			assert(res->length_ == BIGINT_DOUBLE_BITS);
			res->setZero();
		} else {
			res = *result = new BigInt(BIGINT_DOUBLE_BITS);
		}
	} else {
		res = new BigInt(BIGINT_DOUBLE_BITS);
	}

	assert(res != NULL);
	assert(res->length_ == BIGINT_DOUBLE_BITS);

	for(unsigned int i = 0; i < size_; ++i) {
		c = 0;
		for (unsigned int j = 0; j < number.size_; ++j) {
			temp_res = res->blocks_[i + j] + blocks_[i] * number.blocks_[j] + c;
			res->blocks_[i + j] = temp_res & BLOCK_MAX_NUMBER;
			c = (temp_res >> BLOCK_BITS) & BLOCK_MAX_NUMBER;
		}
		res->blocks_[i + number.size_] = c;
	}
	return res;
}

void BigInt::mulByBit(int bitValue)
{
	assert((bitValue & 1) == bitValue);

	if (bitValue == 0) {
		setZero();
	}
}

bool BigInt::div(const BigInt &N, const BigInt &D, BigInt *Q, BigInt *R)
{
	int res;

	if (D.isZero()) {
		WARN("Could not divide by zero.");
		return false;
	}

	Q = new BigInt(BIGINT_BITS);
	R = new BigInt(BIGINT_BITS);

	int i;
	for (i = N.length_ - 1; i >= 0; --i) {
		R->shiftLeftBlock(1);
		R->setBit(0, N.getBit(i));
		res = R->cmp(D);
		if (res == 1 || res == 0) {
			R->sub(D);
			Q->setBit(i, 1);
		}
	}
	return true;
}

void BigInt::mulMont(const BigInt &y, const BigInt &m, BigInt &ret)
{
	assert(length_ == y.length_);
	assert(length_ == m.length_);
	assert(m.preComputedTable_);

	assert(cmp(m) == -1);
	assert(y.cmp(m) == -1);

	// gcd(m; b) = 1
	// b == 2
	// m should be odd
	assert(m.getBit(0) == 1);

	// this - x
	BigInt result(BIGINT_DOUBLE_BITS);

	bool overflow = false;
	unsigned int u = 0;
	unsigned int y0 = y.getBit(0);
	unsigned int xi = 0;
	unsigned int i;

	// fing max len of numbers
	unsigned int len = m.getPosMostSignificatnBit();

	DEBUG("max len is {}", len);

	for (i = 0; i < len; ++i) {
		overflow = false;

		xi = this->getBit(i);
		assert((xi & 1) == xi);
		assert((result.getBit(0) & 1) == result.getBit(0));
		//DEBUG("{})xi = {:x}", i, xi);
		u = (result.getBit(0) + xi * y0) % 2;
		assert((u & 1) == u);

		if (xi) {
			overflow = result.add(y);
			//DEBUG("Overflow A + y = {}", overflow);
		}
		if (u) {
			bool overflow2 = result.add(m);
			//DEBUG("Overflow A + m = {}", overflow2);
			if (overflow) {
				if (overflow2) {
					assert(1 == 0);
				} else {
					assert(2 == 0);
				}
			}
			overflow = overflow2;
		}
		assert(result.getBit(0) == 0);
		result.shiftRightBlock(1);
		if (overflow) {
			//DEBUG("Set overflowed bit");
			result.setBit(length_ - 1, 1);
		}
		//DEBUG("temp {}", A->toString());
	}
	if (result.cmp(m) == 1) {
		result.sub(m);
		//DEBUG("SUB");
	}
	DEBUG("temp {} mostSigBit = {}", result.toString(), result.getPosMostSignificatnBit());
	result.shiftLeft(len);
	DEBUG("temp shift {},  mostSigBit = {}", result.toString(), result.getPosMostSignificatnBit());
	result.mod(m);

	assert(result.getPosMostSignificatnBit() <= BIGINT_BITS);
	ret.copyContent(result);
}

void BigInt::initModularReduction()
{
	assert(isZero() == false);
	assert(preComputedTable_ == NULL);

	int mostSignBit = getPosMostSignificatnBit();
	preComputedTable_ = new BigInt*[mostSignBit + 1];
	int i;

	preComputedTable_[0] = new BigInt(BIGINT_DOUBLE_BITS);
	preComputedTable_[0]->setNumber(1);
	preComputedTable_[0]->shiftLeft(mostSignBit);
	//DEBUG("most sign bit {}", mostSignBit);
	//DEBUG("init shift table[{}] = {}", 0, preComputedTable[0]->toString());
	while(preComputedTable_[0]->cmp(*this) == 1) {
		preComputedTable_[0]->sub(*this);
	}
	//DEBUG("init table[{}] = {}", 0, table[0]->toString());

	for (i = 1; i <= mostSignBit; ++i) {
		preComputedTable_[i] = preComputedTable_[i - 1]->copy();
		preComputedTable_[i]->shiftLeft(1);
		while(preComputedTable_[i]->cmp(*this) == 1) {
			preComputedTable_[i]->sub(*this);
		}
		//DEBUG("init table[{}] = {}", i, table[i]->toString());
	}
	INFO("Init of montgomery multiplication done.");
}

void BigInt::shutDownModularReduction()
{
	assert(preComputedTable_);

	int mostSignBit = getPosMostSignificatnBit();

	int i;
	for (i = 0; i <= mostSignBit; ++i) {
		//DEBUG("shutdown table[{}] = {}", i, table[i]->toString());
		delete preComputedTable_[i];
	}
	delete[] preComputedTable_;
	preComputedTable_ = NULL;
	INFO("Shut down of montgomery multiplication done.");
}

void BigInt::mod(const BigInt &m)
{
	assert(m.preComputedTable_);

	BigInt r(BIGINT_DOUBLE_BITS);
	int k = m.getPosMostSignificatnBit();
	int posMostSignBitZ = getPosMostSignificatnBit();

	assert(posMostSignBitZ <= 2 * k);

	if (cmp(m) == -1) {
		return;
	}
	if (posMostSignBitZ == k) {
		sub(m);
		return;
	}
	int i;
	for (i = posMostSignBitZ; i >= k; --i) {
		if (clearBit(i)) {
			r.add(*(m.preComputedTable_[i - k]));
		}
	}
	r.add(*this);

	while (r.cmp(m) == 1) {
		r.sub(m);
	}
	copyContent(r);
}

void BigInt::splitToRWords(std::vector<block> &rWords, int lenBits)
{
	assert(lenBits > 0 && lenBits <= WORD_BITS);
	int len = length_ / WORD_BITS;
	int maxValue = fillBits(lenBits);
	std::vector<block> rawArray(len);
	blocksToRawArray(rawArray);

	int bitValue;
	int posBlock;
	int rWord = 0;
	int rWordBitPos = lenBits - 1;
	int leftBits;

	for (posBlock = len - 1; posBlock >= 0; --posBlock) {
		for (leftBits = WORD_BITS - 1; leftBits >= 0; --leftBits) {
			bitValue = (rawArray[posBlock] >> leftBits) & 1;
			//DEBUG("before rWord = {}", rWord);
			// set bit to rWord
			rWord ^= (-bitValue ^ rWord) & (1 << rWordBitPos);
			//DEBUG("after rWord = {}", rWord);
			if (rWordBitPos != 0) {
				// move to other bit position
				--rWordBitPos;
			} else {
				// push rWord to vector and clear data
				rWordBitPos = lenBits - 1;
				//DEBUG("rWord = {}, but possible max is {}", rWord, maxValue);
				assert(rWord <= maxValue);
				rWords.push_back(rWord);
				rWord = 0;
			}
		}
	}
	if (rWordBitPos != lenBits - 1) {
		rWord >>= rWordBitPos + 1;
		//DEBUG("last shift to rWordBitPos = {} , rWord = {}", rWordBitPos + 1, rWord);
		rWords.push_back(rWord);
	}
	std::reverse(rWords.begin(), rWords.end());
}

void BigInt::exp(const BigInt &e, const BigInt &m)
{
	BigInt a;
	a.setZero();

	std::vector<block> rWords;
	a.splitToRWords(rWords, 5);
	unsigned int i;
	for (i = 0; i < rWords.size(); ++i) {
		DEBUG("rWords[{}] = {}",i, rWords[i]);
	}
}
