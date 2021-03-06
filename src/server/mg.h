#ifndef K_MG_H_
#define K_MG_H_

namespace K {
  mLevels mgLevelsFilter;
  vector<mTrade> mgTrades;
  double mgFairValue = 0;
  double mgEwmaL = 0;
  double mgEwmaM = 0;
  double mgEwmaS = 0;
  double mgEwmaP = 0;
  vector<double> mgSMA3;
  vector<double> mgStatFV;
  vector<double> mgStatBid;
  vector<double> mgStatAsk;
  vector<double> mgStatTop;
  double mgStdevFV = 0;
  double mgStdevFVMean = 0;
  double mgStdevBid = 0;
  double mgStdevBidMean = 0;
  double mgStdevAsk = 0;
  double mgStdevAskMean = 0;
  double mgStdevTop = 0;
  double mgStdevTopMean = 0;
  double mgTargetPos = 0;
  class MG {
    public:
      static void main() {
        load();
        waitData();
        waitUser();
      };
      static bool empty() {
        return (!mgLevelsFilter.bids.size() or !mgLevelsFilter.asks.size());
      };
      static void calcStats() {
        static int mgT = 0;
        if (++mgT == 60) {
          mgT = 0;
          ewmaPUp();
          ewmaUp();
        }
        stdevPUp();
      };
      static void calcFairValue() {
        if (empty()) return;
        double mgFairValue_ = mgFairValue;
        double topAskPrice = mgLevelsFilter.asks.begin()->price;
        double topBidPrice = mgLevelsFilter.bids.begin()->price;
        double topAskSize = mgLevelsFilter.asks.begin()->size;
        double topBidSize = mgLevelsFilter.bids.begin()->size;
        if (!topAskPrice or !topBidPrice or !topAskSize or !topBidSize) return;
        mgFairValue = FN::roundNearest(
          qp.fvModel == mFairValueModel::BBO
            ? (topAskPrice + topBidPrice) / 2
            : (topAskPrice * topAskSize + topBidPrice * topBidSize) / (topAskSize + topBidSize),
          gw->minTick
        );
        if (!mgFairValue or (mgFairValue_ and abs(mgFairValue - mgFairValue_) < gw->minTick)) return;
        ev_gwDataWallet(mWallet());
        UI::uiSend(uiTXT::FairValue, {{"price", mgFairValue}}, true);
      };
    private:
      static void load() {
        json k = DB::load(uiTXT::MarketData);
        if (k.size()) {
          for (json::iterator it = k.begin(); it != k.end(); ++it) {
            if (it->value("time", (unsigned long)0)+qp.quotingStdevProtectionPeriods*1e+3<FN::T()) continue;
            mgStatFV.push_back(it->value("fv", 0.0));
            mgStatBid.push_back(it->value("bid", 0.0));
            mgStatAsk.push_back(it->value("ask", 0.0));
            mgStatTop.push_back(it->value("bid", 0.0));
            mgStatTop.push_back(it->value("ask", 0.0));
          }
          calcStdev();
        }
        FN::log("DB", string("loaded ") + to_string(mgStatFV.size()) + " STDEV Periods");
        if (argEwmaLong) mgEwmaL = argEwmaLong;
        if (argEwmaMedium) mgEwmaM = argEwmaMedium;
        if (argEwmaShort) mgEwmaS = argEwmaShort;
        k = DB::load(uiTXT::EWMAChart);
        if (k.size()) {
          k = k.at(0);
          if (!mgEwmaL and k.value("time", (unsigned long)0)+qp.longEwmaPeriods*6e+4>FN::T())
            mgEwmaL = k.value("ewmaLong", 0.0);
          if (!mgEwmaM and k.value("time", (unsigned long)0)+qp.mediumEwmaPeriods*6e+4>FN::T())
            mgEwmaM = k.value("ewmaMedium", 0.0);
          if (!mgEwmaS and k.value("time", (unsigned long)0)+qp.shortEwmaPeriods*6e+4>FN::T())
            mgEwmaS = k.value("ewmaShort", 0.0);
        }
        FN::log(argEwmaLong ? "ARG" : "DB", string("loaded EWMA Long = ") + to_string(mgEwmaL));
        FN::log(argEwmaMedium ? "ARG" : "DB", string("loaded EWMA Medium = ") + to_string(mgEwmaM));
        FN::log(argEwmaShort ? "ARG" : "DB", string("loaded EWMA Short = ") + to_string(mgEwmaS));
      };
      static void waitData() {
        ev_gwDataTrade = [](mTrade k) {
          if (argDebugEvents) FN::log("DEBUG", "EV MG ev_gwDataTrade");
          tradeUp(k);
        };
        ev_gwDataLevels = [](mLevels k) {
          if (argDebugEvents) FN::log("DEBUG", "EV MG ev_gwDataLevels");
          levelUp(k);
        };
      };
      static void waitUser() {
        UI::uiSnap(uiTXT::MarketTrade, &onSnapTrade);
        UI::uiSnap(uiTXT::FairValue, &onSnapFair);
        UI::uiSnap(uiTXT::EWMAChart, &onSnapEwma);
      };
      static json onSnapTrade() {
        json k;
        for (unsigned i=0; i<mgTrades.size(); ++i)
          k.push_back(mgTrades[i]);
        return k;
      };
      static json onSnapFair() {
        return {{{"price", mgFairValue}}};
      };
      static json onSnapEwma() {
        return {{
          {"stdevWidth", {
            {"fv", mgStdevFV},
            {"fvMean", mgStdevFVMean},
            {"tops", mgStdevTop},
            {"topsMean", mgStdevTopMean},
            {"bid", mgStdevBid},
            {"bidMean", mgStdevBidMean},
            {"ask", mgStdevAsk},
            {"askMean", mgStdevAskMean}
          }},
          {"ewmaQuote", mgEwmaP},
          {"ewmaShort", mgEwmaS},
          {"ewmaMedium", mgEwmaM},
          {"ewmaLong", mgEwmaL},
          {"fairValue", mgFairValue}
        }};
      };
      static void stdevPUp() {
        if (empty()) return;
        double topBid = mgLevelsFilter.bids.begin()->price;
        double topAsk = mgLevelsFilter.bids.begin()->price;
        if (!topBid or !topAsk) return;
        mgStatFV.push_back(mgFairValue);
        mgStatBid.push_back(topBid);
        mgStatAsk.push_back(topAsk);
        mgStatTop.push_back(topBid);
        mgStatTop.push_back(topAsk);
        calcStdev();
        DB::insert(uiTXT::MarketData, {
          {"fv", mgFairValue},
          {"bid", topBid},
          {"ask", topAsk},
          {"time", FN::T()},
        }, false, "NULL", FN::T() - 1e+3 * qp.quotingStdevProtectionPeriods);
      };
      static void tradeUp(mTrade k) {
        k.exchange = gw->exchange;
        k.pair = mPair(gw->base, gw->quote);
        k.time = FN::T();
        mgTrades.push_back(k);
        if (mgTrades.size()>69) mgTrades.erase(mgTrades.begin());
        UI::uiSend(uiTXT::MarketTrade, k);
      };
      static void levelUp(mLevels k) {
        static unsigned long lastUp = 0;
        filter(k);
        if (lastUp+369 > FN::T()) return;
        UI::uiSend(uiTXT::MarketData, k, true);
        lastUp = FN::T();
      };
      static void ewmaUp() {
        calcEwma(&mgEwmaL, qp.longEwmaPeriods);
        calcEwma(&mgEwmaM, qp.mediumEwmaPeriods);
        calcEwma(&mgEwmaS, qp.shortEwmaPeriods);
        calcTargetPos();
        ev_mgTargetPosition();
        UI::uiSend(uiTXT::EWMAChart, {
          {"stdevWidth", {
            {"fv", mgStdevFV},
            {"fvMean", mgStdevFVMean},
            {"tops", mgStdevTop},
            {"topsMean", mgStdevTopMean},
            {"bid", mgStdevBid},
            {"bidMean", mgStdevBidMean},
            {"ask", mgStdevAsk},
            {"askMean", mgStdevAskMean}
          }},
          {"ewmaQuote", mgEwmaP},
          {"ewmaShort", mgEwmaS},
          {"ewmaMedium", mgEwmaM},
          {"ewmaLong", mgEwmaL},
          {"fairValue", mgFairValue}
        }, true);
        DB::insert(uiTXT::EWMAChart, {
          {"ewmaLong", mgEwmaL},
          {"ewmaMedium", mgEwmaM},
          {"ewmaShort", mgEwmaS},
          {"time", FN::T()}
        });
      };
      static void ewmaPUp() {
        calcEwma(&mgEwmaP, qp.quotingEwmaProtectionPeriods);
        ev_mgEwmaQuoteProtection();
      };
      static void filter(mLevels k) {
        mgLevelsFilter = k;
        if (empty()) return;
        ogMutex.lock();
        for (map<string, mOrder>::iterator it = allOrders.begin(); it != allOrders.end(); ++it)
          filter(mSide::Bid == it->second.side ? &mgLevelsFilter.bids : &mgLevelsFilter.asks, it->second);
        ogMutex.unlock();
        if (!empty()) {
          calcFairValue();
          ev_mgLevels();
        }
      };
      static void filter(vector<mLevel>* k, mOrder o) {
        for (vector<mLevel>::iterator it = k->begin(); it != k->end();)
          if (abs(it->price - o.price) < gw->minTick) {
            it->size = it->size - o.quantity;
            if (it->size < gw->minTick) k->erase(it);
            break;
          } else ++it;
      };
      static void cleanStdev() {
        size_t periods = (size_t)qp.quotingStdevProtectionPeriods;
        if (mgStatFV.size()>periods) mgStatFV.erase(mgStatFV.begin(), mgStatFV.end()-periods);
        if (mgStatBid.size()>periods) mgStatBid.erase(mgStatBid.begin(), mgStatBid.end()-periods);
        if (mgStatAsk.size()>periods) mgStatAsk.erase(mgStatAsk.begin(), mgStatAsk.end()-periods);
        if (mgStatTop.size()>periods*2) mgStatTop.erase(mgStatTop.begin(), mgStatTop.end()-(periods*2));
      };
      static void calcStdev() {
        cleanStdev();
        if (mgStatFV.size() < 2 or mgStatBid.size() < 2 or mgStatAsk.size() < 2 or mgStatTop.size() < 4) return;
        double k = qp.quotingStdevProtectionFactor;
        mgStdevFV = calcStdev(mgStatFV, k, &mgStdevFVMean);
        mgStdevBid = calcStdev(mgStatBid, k, &mgStdevBidMean);
        mgStdevAsk = calcStdev(mgStatAsk, k, &mgStdevAskMean);
        mgStdevTop = calcStdev(mgStatTop, k, &mgStdevTopMean);
      };
      static double calcStdev(vector<double> a, double f, double *mean) {
        int n = a.size();
        if (n == 0) return 0.0;
        double sum = 0;
        for (int i = 0; i < n; ++i) sum += a[i];
        *mean = sum / n;
        double sq_diff_sum = 0;
        for (int i = 0; i < n; ++i) {
          double diff = a[i] - *mean;
          sq_diff_sum += diff * diff;
        }
        double variance = sq_diff_sum / n;
        return sqrt(variance) * f;
      };
      static void calcEwma(double *k, int periods) {
        if (*k) {
          double alpha = (double)2 / (periods + 1);
          *k = alpha * mgFairValue + (1 - alpha) * *k;
        } else *k = mgFairValue;
      };
      static void calcTargetPos() {
        mgSMA3.push_back(mgFairValue);
        if (mgSMA3.size()>3) mgSMA3.erase(mgSMA3.begin(), mgSMA3.end()-3);
        double SMA3 = 0;
        for (vector<double>::iterator it = mgSMA3.begin(); it != mgSMA3.end(); ++it)
          SMA3 += *it;
        SMA3 /= mgSMA3.size();
        double newTargetPosition = 0;
        if (qp.autoPositionMode == mAutoPositionMode::EWMA_LMS) {
          double newTrend = ((SMA3 * 100 / mgEwmaL) - 100);
          double newEwmacrossing = ((mgEwmaS * 100 / mgEwmaM) - 100);
          newTargetPosition = ((newTrend + newEwmacrossing) / 2) * (1 / qp.ewmaSensiblityPercentage);
        } else if (qp.autoPositionMode == mAutoPositionMode::EWMA_LS)
          newTargetPosition = ((mgEwmaS * 100/ mgEwmaL) - 100) * (1 / qp.ewmaSensiblityPercentage);
        if (newTargetPosition > 1) newTargetPosition = 1;
        else if (newTargetPosition < -1) newTargetPosition = -1;
        mgTargetPos = newTargetPosition;
      };
  };
}

#endif
