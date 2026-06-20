import yfinance as yf

ticker = input("Enter stock ticker (e.g. AAPL): ").strip().upper()
period = input("Enter period (e.g. 1y, 6mo, 3mo, 1mo): ").strip() or "1y"

stock = yf.Ticker(ticker)
# Fetch data cleanly
hist = stock.history(period=period)

if hist.empty:
    print(f"No data found for {ticker}")
else:
    hist = hist.reset_index()
    
    # Clean up column names 
    hist.columns = [c.replace(" ", "_") for c in hist.columns]
    
    # Ensure Date format is strictly YYYY-MM-DD (removes timezone strings)
    hist['Date'] = hist['Date'].dt.strftime('%Y-%m-%d')
    
    # Create an Adj_Close column at index 5 to match the standard web CSV layout
    hist['Adj_Close'] = hist['Close']
    
    # Reorder columns explicitly so Volume is at column index 6 (7th item)
    hist = hist[['Date', 'Open', 'High', 'Low', 'Close', 'Adj_Close', 'Volume']]
    
    filename = f"{ticker}.csv"
    hist.to_csv(filename, index=False)
    print(f"Saved {filename} — {len(hist)} rows matching C-Parser schema.")
